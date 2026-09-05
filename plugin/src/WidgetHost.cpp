#include "WidgetHost.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>

#include <objbase.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
	constexpr const char* VIEW_PATH = "iwantwidgets/index.html";

	// The Flash original prefixed every filename with /Interface/exported/.
	constexpr const char* ROOT_PREFIX = "Interface/exported/";

	std::string NormalizePath(const std::string& file)
	{
		std::string p = file;
		std::replace(p.begin(), p.end(), '\\', '/');
		while (!p.empty() && p.front() == '/') {
			p.erase(p.begin());
		}
		p = std::string(ROOT_PREFIX) + p;
		// BSResource wants Windows separators; forward slashes fail to
		// resolve loose files.
		std::replace(p.begin(), p.end(), '/', '\\');
		return p;
	}

	std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	// Reads through the game's resource system, so both loose files and BSA
	// archives resolve. Path is relative to Data\ with backslash separators.
	bool ReadGameFile(const std::string& relPath, std::vector<std::uint8_t>& out)
	{
		{
			RE::BSResourceNiBinaryStream stream(relPath);
			if (stream.good()) {
				constexpr std::size_t CHUNK = 64 * 1024;
				std::uint8_t buf[CHUNK];
				std::size_t got = 0;
				do {
					got = stream.read(buf, CHUNK);
					out.insert(out.end(), buf, buf + got);
				} while (got == CHUNK);
				if (!out.empty()) {
					return true;
				}
			}
		}
		// Fallback: plain loose-file read relative to the game directory.
		// Still VFS-aware under MO2 (the hook is process-wide).
		std::ifstream f("Data\\" + relPath, std::ios::binary);
		if (f) {
			out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
		}
		return !out.empty();
	}

	std::string Base64(const std::uint8_t* data, std::size_t len)
	{
		static constexpr char TBL[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(((len + 2) / 3) * 4);
		std::size_t i = 0;
		for (; i + 2 < len; i += 3) {
			const std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
			out.push_back(TBL[(n >> 18) & 63]);
			out.push_back(TBL[(n >> 12) & 63]);
			out.push_back(TBL[(n >> 6) & 63]);
			out.push_back(TBL[n & 63]);
		}
		if (i + 1 == len) {
			const std::uint32_t n = data[i] << 16;
			out.push_back(TBL[(n >> 18) & 63]);
			out.push_back(TBL[(n >> 12) & 63]);
			out.append("==");
		} else if (i + 2 == len) {
			const std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
			out.push_back(TBL[(n >> 18) & 63]);
			out.push_back(TBL[(n >> 12) & 63]);
			out.push_back(TBL[(n >> 6) & 63]);
			out.push_back('=');
		}
		return out;
	}

	void EnsureCom()
	{
		// WIC needs COM on the calling thread; natives arrive on arbitrary
		// Papyrus VM threads.
		thread_local bool initialized = [] {
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			return true;
		}();
		(void)initialized;
	}

	// Decode DDS to raw RGBA via Windows' built-in WIC DDS codec (covers
	// DXT1/3/5 and DX10-header formats), with a manual fallback for legacy
	// uncompressed 32bpp DDS files the codec rejects.
	bool DecodeDDSToRGBA(const std::vector<std::uint8_t>& dds, std::vector<std::uint8_t>& rgba,
		int& w, int& h)
	{
		EnsureCom();

		ComPtr<IWICImagingFactory> factory;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&factory)))) {
			return false;
		}

		ComPtr<IStream> in(SHCreateMemStream(dds.data(), static_cast<UINT>(dds.size())));
		ComPtr<IWICBitmapDecoder> decoder;
		if (in && SUCCEEDED(factory->CreateDecoderFromStream(in.Get(), nullptr,
						WICDecodeMetadataCacheOnDemand, &decoder))) {
			ComPtr<IWICBitmapFrameDecode> frame;
			ComPtr<IWICBitmapSource> converted;
			UINT uw = 0, uh = 0;
			if (SUCCEEDED(decoder->GetFrame(0, &frame)) &&
				SUCCEEDED(frame->GetSize(&uw, &uh)) &&
				SUCCEEDED(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame.Get(),
					&converted))) {
				rgba.resize(static_cast<std::size_t>(uw) * uh * 4);
				if (SUCCEEDED(converted->CopyPixels(nullptr, uw * 4,
						static_cast<UINT>(rgba.size()), rgba.data()))) {
					w = static_cast<int>(uw);
					h = static_cast<int>(uh);
					return true;
				}
			}
		}

		// Legacy uncompressed 32bpp DDS: 4-byte magic + 124-byte header, then
		// raw pixel data. Header fields: height @12, width @16, pixel format
		// flags @80 (0x40 = uncompressed RGB), bit count @88, R mask @92,
		// alpha mask @104.
		if (dds.size() > 128 && std::memcmp(dds.data(), "DDS ", 4) == 0) {
			const auto u32 = [&](std::size_t off) {
				std::uint32_t v;
				std::memcpy(&v, dds.data() + off, 4);
				return v;
			};
			const std::uint32_t height = u32(12), width = u32(16);
			const std::uint32_t pfFlags = u32(80), bitCount = u32(88);
			const std::uint32_t rMask = u32(92), aMask = u32(104);
			const std::size_t need = 128 + static_cast<std::size_t>(width) * height * 4;
			const bool bgra = rMask == 0x00FF0000u;
			const bool already = rMask == 0x000000FFu;
			if ((pfFlags & 0x40) != 0 && bitCount == 32 && (bgra || already) &&
				dds.size() >= need) {
				const std::uint8_t* src = dds.data() + 128;
				const std::size_t count = static_cast<std::size_t>(width) * height;
				rgba.resize(count * 4);
				for (std::size_t i = 0; i < count; ++i) {
					const std::uint8_t* p = src + i * 4;
					std::uint8_t* q = rgba.data() + i * 4;
					if (bgra) {
						q[0] = p[2];
						q[1] = p[1];
						q[2] = p[0];
					} else {
						q[0] = p[0];
						q[1] = p[1];
						q[2] = p[2];
					}
					q[3] = aMask ? p[3] : 255;
				}
				w = static_cast<int>(width);
				h = static_cast<int>(height);
				return true;
			}
		}

		return false;
	}
}

namespace
{
	// Menus that hide the vanilla HUD (and with it, the Flash widgets this
	// mod replaces). MessageBoxMenu and the fader/cursor menus deliberately
	// aren't listed - the HUD stays visible under those.
	constexpr const char* HIDE_MENUS[] = {
		"Dialogue Menu", "Console", "InventoryMenu", "MagicMenu", "MapMenu",
		"StatsMenu", "ContainerMenu", "BarterMenu", "GiftMenu", "Training Menu",
		"Lockpicking Menu", "Book Menu", "Crafting Menu", "FavoritesMenu",
		"Journal Menu", "Sleep/Wait Menu", "LevelUp Menu", "Main Menu",
		"Loading Menu", "RaceSex Menu", "TweenMenu", "Mist Menu"
	};

	class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuWatcher* GetSingleton()
		{
			static MenuWatcher instance;
			return &instance;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* ev,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!ev) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const std::string_view name = ev->menuName.c_str();
			bool relevant = false;
			for (const char* m : HIDE_MENUS) {
				if (name == m) {
					relevant = true;
					break;
				}
			}
			if (relevant) {
				std::scoped_lock lock(mtx_);
				if (ev->opening) {
					open_.insert(std::string(name));
				} else {
					open_.erase(std::string(name));
				}
				WidgetHost::Get().SetMenusClear(open_.empty());
			}
			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		std::mutex mtx_;
		std::set<std::string> open_;
	};
}

WidgetHost& WidgetHost::Get()
{
	static WidgetHost instance;
	return instance;
}

void WidgetHost::OnDataLoaded()
{
	api_ = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
	if (!api_) {
		logger::critical("PrismaUI.dll not found or API request failed - widgets will not render");
		return;
	}

	view_ = api_->CreateView(VIEW_PATH, [](PrismaView) {
		auto& host = WidgetHost::Get();
		logger::info("iWant Widgets view DOM ready");
		std::vector<std::string> backlog;
		{
			std::scoped_lock lock(host.mtx_);
			host.domReady_ = true;
			backlog.swap(host.pending_);
		}
		for (auto& json : backlog) {
			host.Dispatch(json);
		}
	});

	api_->RegisterJSListener(view_, "iwMetrics", [](const char* arg) {
		int id = 0, w = 0, h = 0;
		if (arg && sscanf_s(arg, R"({"id":%d,"w":%d,"h":%d})", &id, &w, &h) == 3) {
			WidgetHost::Get().SetMetrics(id, w, h);
		}
	});

	if (auto* ui = RE::UI::GetSingleton()) {
		ui->AddEventSink(MenuWatcher::GetSingleton());
	}

	// There is no event for the global menus-shown flag (`tm`, and native
	// HUD-hiders like SexLab's Hide HUD flip it directly), so poll it.
	std::thread([]() {
		for (;;) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			if (auto* ui = RE::UI::GetSingleton()) {
				WidgetHost::Get().SetGameHudShown(ui->IsShowingMenus());
			}
		}
	}).detach();

	logger::info("iWant Widgets view created ({})", VIEW_PATH);
}

void WidgetHost::SetMenusClear(bool clear)
{
	menusClear_ = clear;
	ApplyVisibility();
}

void WidgetHost::SetGameHudShown(bool shown)
{
	if (gameHudShown_.exchange(shown) != shown) {
		ApplyVisibility();
	}
}

void WidgetHost::ApplyVisibility()
{
	const bool visible = menusClear_.load() && gameHudShown_.load();
	if (overlayVisible_.exchange(visible) == visible) {
		return;
	}
	SKSE::GetTaskInterface()->AddTask([visible]() {
		auto& host = WidgetHost::Get();
		if (host.api_ && host.view_ && host.api_->IsValid(host.view_)) {
			if (visible) {
				host.api_->Show(host.view_);
			} else {
				host.api_->Hide(host.view_);
			}
		}
	});
}

bool WidgetHost::IsReady() const
{
	return api_ != nullptr && domReady_.load();
}

int WidgetHost::NextId()
{
	return nextId_.fetch_add(1);
}

int WidgetHost::PeekNextId() const
{
	return nextId_.load();
}

void WidgetHost::Send(std::string json)
{
	{
		std::scoped_lock lock(mtx_);
		if (!domReady_.load()) {
			pending_.push_back(std::move(json));
			return;
		}
	}
	Dispatch(json);
}

void WidgetHost::Dispatch(const std::string& json)
{
	if (!api_ || !view_) {
		return;
	}
	// Serialize all view access onto the main thread; natives run on Papyrus
	// VM threads and Ultralight is not documented thread-safe.
	SKSE::GetTaskInterface()->AddTask([json]() {
		auto& host = WidgetHost::Get();
		if (host.api_ && host.view_ && host.api_->IsValid(host.view_)) {
			host.api_->InteropCall(host.view_, "iwCall", json.c_str());
		}
	});
}

const WidgetHost::ImageData& WidgetHost::LoadImageFile(const std::string& file)
{
	const std::string key = ToLower(file);
	{
		std::scoped_lock lock(mtx_);
		if (auto it = imageCache_.find(key); it != imageCache_.end()) {
			return it->second;
		}
	}

	ImageData data;
	const std::string relPath = NormalizePath(file);
	const std::string ext = ToLower(std::filesystem::path(relPath).extension().string());

	std::vector<std::uint8_t> bytes;
	if (!ReadGameFile(relPath, bytes)) {
		logger::error("loadWidget: cannot read '{}' (loose or BSA)", relPath);
	} else if (ext == ".dds") {
		std::vector<std::uint8_t> rgba;
		if (DecodeDDSToRGBA(bytes, rgba, data.w, data.h)) {
			data.px = Base64(rgba.data(), rgba.size());
		} else {
			logger::error("loadWidget: DDS decode failed for '{}'", relPath);
		}
	} else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
		const char* mime = (ext == ".png") ? "image/png" : "image/jpeg";
		data.url = std::format("data:{};base64,{}", mime, Base64(bytes.data(), bytes.size()));
	} else {
		// .swf widgets cannot be rendered outside Scaleform.
		logger::error("loadWidget: unsupported format '{}' for '{}'", ext, relPath);
	}

	if (data.px.empty() && data.url.empty()) {
		// 1x1 transparent pixel so a bad path degrades to an invisible widget
		// instead of a broken-image glyph.
		data.px = "AAAAAA==";
		data.w = 1;
		data.h = 1;
	}

	std::scoped_lock lock(mtx_);
	return imageCache_.emplace(key, std::move(data)).first->second;
}

void WidgetHost::SetMetrics(int id, int w, int h)
{
	std::scoped_lock lock(mtx_);
	metrics_[id] = { w, h };
}

std::pair<int, int> WidgetHost::GetMetrics(int id) const
{
	std::scoped_lock lock(mtx_);
	if (auto it = metrics_.find(id); it != metrics_.end()) {
		return it->second;
	}
	return { 0, 0 };
}

namespace Json
{
	std::string Escape(std::string_view s)
	{
		std::string out;
		out.reserve(s.size() + 8);
		for (const char c : s) {
			switch (c) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(c) < 0x20) {
					out += std::format("\\u{:04x}", static_cast<unsigned char>(c));
				} else {
					out += c;
				}
			}
		}
		return out;
	}

	void Obj::Comma()
	{
		if (!body_.empty()) {
			body_ += ',';
		}
	}

	Obj& Obj::Str(std::string_view key, std::string_view value)
	{
		Comma();
		body_ += std::format(R"("{}":"{}")", key, Escape(value));
		return *this;
	}

	Obj& Obj::Num(std::string_view key, double value)
	{
		Comma();
		body_ += std::format(R"("{}":{})", key, value);
		return *this;
	}

	Obj& Obj::Int(std::string_view key, std::int64_t value)
	{
		Comma();
		body_ += std::format(R"("{}":{})", key, value);
		return *this;
	}

	Obj& Obj::Boolean(std::string_view key, bool value)
	{
		Comma();
		body_ += std::format(R"("{}":{})", key, value ? "true" : "false");
		return *this;
	}

	Obj& Obj::IntArray(std::string_view key, const std::vector<std::int32_t>& values)
	{
		Comma();
		body_ += std::format(R"("{}":[)", key);
		for (std::size_t i = 0; i < values.size(); ++i) {
			if (i) {
				body_ += ',';
			}
			body_ += std::to_string(values[i]);
		}
		body_ += ']';
		return *this;
	}

	std::string Obj::Build()
	{
		return "{" + body_ + "}";
	}
}
