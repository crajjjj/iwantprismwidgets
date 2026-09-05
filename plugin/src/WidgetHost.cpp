#include "WidgetHost.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <regex>
#include <set>

#include <propidl.h>

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

	UINT ReadMetaUInt(IWICMetadataQueryReader* reader, const wchar_t* name, UINT fallback)
	{
		if (!reader) {
			return fallback;
		}
		PROPVARIANT v;
		PropVariantInit(&v);
		UINT out = fallback;
		if (SUCCEEDED(reader->GetMetadataByName(name, &v))) {
			switch (v.vt) {
			case VT_UI1:
				out = v.bVal;
				break;
			case VT_UI2:
				out = v.uiVal;
				break;
			case VT_UI4:
				out = v.ulVal;
				break;
			}
		}
		PropVariantClear(&v);
		return out;
	}

	constexpr std::size_t MAX_ANIM_FRAMES = 64;

	// Extract an animated GIF as fully-composited RGBA frames. GIF frames are
	// partial rectangles layered per a disposal method (1 keep, 2 clear the
	// frame rect, 3 restore the pre-frame canvas), so each emitted frame is
	// the composited logical screen. Returns false for still GIFs.
	bool DecodeGifFrames(const std::vector<std::uint8_t>& bytes, WidgetHost::ImageData& data)
	{
		EnsureCom();

		ComPtr<IWICImagingFactory> factory;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
				IID_PPV_ARGS(&factory)))) {
			return false;
		}
		ComPtr<IStream> in(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
		ComPtr<IWICBitmapDecoder> decoder;
		if (!in || FAILED(factory->CreateDecoderFromStream(in.Get(), nullptr,
					  WICDecodeMetadataCacheOnDemand, &decoder))) {
			return false;
		}

		UINT count = 0;
		if (FAILED(decoder->GetFrameCount(&count)) || count <= 1) {
			return false;
		}
		count = static_cast<UINT>(std::min<std::size_t>(count, MAX_ANIM_FRAMES));

		UINT sw = 0, sh = 0;
		{
			ComPtr<IWICMetadataQueryReader> mr;
			if (SUCCEEDED(decoder->GetMetadataQueryReader(&mr))) {
				sw = ReadMetaUInt(mr.Get(), L"/logscrdesc/Width", 0);
				sh = ReadMetaUInt(mr.Get(), L"/logscrdesc/Height", 0);
			}
		}

		std::vector<std::uint8_t> canvas;
		for (UINT i = 0; i < count; ++i) {
			ComPtr<IWICBitmapFrameDecode> frame;
			if (FAILED(decoder->GetFrame(i, &frame))) {
				break;
			}
			UINT fw = 0, fh = 0;
			frame->GetSize(&fw, &fh);

			UINT left = 0, top = 0, delayCs = 10, disposal = 0;
			{
				ComPtr<IWICMetadataQueryReader> fr;
				if (SUCCEEDED(frame->GetMetadataQueryReader(&fr))) {
					left = ReadMetaUInt(fr.Get(), L"/imgdesc/Left", 0);
					top = ReadMetaUInt(fr.Get(), L"/imgdesc/Top", 0);
					delayCs = ReadMetaUInt(fr.Get(), L"/grctlext/Delay", 10);
					disposal = ReadMetaUInt(fr.Get(), L"/grctlext/Disposal", 0);
				}
			}

			if (i == 0) {
				if (!sw || !sh) {
					sw = left + fw;
					sh = top + fh;
				}
				canvas.assign(static_cast<std::size_t>(sw) * sh * 4, 0);
			}

			ComPtr<IWICBitmapSource> conv;
			if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame.Get(),
					&conv))) {
				break;
			}
			std::vector<std::uint8_t> fpix(static_cast<std::size_t>(fw) * fh * 4);
			if (FAILED(conv->CopyPixels(nullptr, fw * 4, static_cast<UINT>(fpix.size()),
					fpix.data()))) {
				break;
			}

			std::vector<std::uint8_t> before;
			if (disposal == 3) {
				before = canvas;
			}

			for (UINT y = 0; y < fh && top + y < sh; ++y) {
				for (UINT x = 0; x < fw && left + x < sw; ++x) {
					const std::uint8_t* s = &fpix[(static_cast<std::size_t>(y) * fw + x) * 4];
					if (s[3] != 0) {
						std::uint8_t* d = &canvas
							[((static_cast<std::size_t>(top) + y) * sw + left + x) * 4];
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = s[3];
					}
				}
			}

			// Sub-2cs delays are treated as 100ms by every renderer out there.
			const int ms = (delayCs < 2) ? 100 : static_cast<int>(delayCs) * 10;
			data.frames.push_back({ ms, Base64(canvas.data(), canvas.size()) });

			if (disposal == 2) {
				const UINT clearW = (std::min)(fw, sw - (std::min)(left, sw));
				for (UINT y = 0; y < fh && top + y < sh; ++y) {
					std::memset(&canvas[((static_cast<std::size_t>(top) + y) * sw + left) * 4],
						0, static_cast<std::size_t>(clearW) * 4);
				}
			} else if (disposal == 3) {
				canvas = std::move(before);
			}
		}

		if (data.frames.size() <= 1) {
			data.frames.clear();
			return false;
		}
		data.w = static_cast<int>(sw);
		data.h = static_cast<int>(sh);
		return true;
	}

	// Spritesheet convention: a filename ending `_<N>f[@<ms>].<ext>` is cut
	// into N frames - stacked vertically when the height divides evenly,
	// side-by-side otherwise. Works for any decodable format (DDS included).
	bool SliceSpritesheet(const std::string& file, const std::vector<std::uint8_t>& rgba,
		int w, int h, WidgetHost::ImageData& data)
	{
		// Digit counts are bounded so std::stoi cannot throw out_of_range on a
		// pathological filename (an uncaught throw here would crash the game).
		// Frame counts beyond 3 digits exceed MAX_ANIM_FRAMES anyway.
		static const std::regex animRe(R"(_(\d{1,3})f(?:@(\d{1,5}))?\.[^.\\/]+$)",
			std::regex::icase);
		std::smatch m;
		if (!std::regex_search(file, m, animRe)) {
			return false;
		}
		const int n = std::clamp(std::stoi(m[1].str()), 2,
			static_cast<int>(MAX_ANIM_FRAMES));
		const int ms = std::clamp(m[2].matched ? std::stoi(m[2].str()) : 100, 20, 10000);

		if (h % n == 0) {
			const int fh = h / n;
			const std::size_t frameBytes = static_cast<std::size_t>(w) * fh * 4;
			for (int i = 0; i < n; ++i) {
				data.frames.push_back({ ms, Base64(rgba.data() + i * frameBytes, frameBytes) });
			}
			data.w = w;
			data.h = fh;
		} else if (w % n == 0) {
			const int fw = w / n;
			std::vector<std::uint8_t> fpix(static_cast<std::size_t>(fw) * h * 4);
			for (int i = 0; i < n; ++i) {
				for (int y = 0; y < h; ++y) {
					std::memcpy(&fpix[static_cast<std::size_t>(y) * fw * 4],
						&rgba[(static_cast<std::size_t>(y) * w + i * fw) * 4],
						static_cast<std::size_t>(fw) * 4);
				}
				data.frames.push_back({ ms, Base64(fpix.data(), fpix.size()) });
			}
			data.w = fw;
			data.h = h;
		} else {
			logger::error(
				"loadWidget: '{}' declares {} frames but neither {}x{} dimension divides",
				file, n, w, h);
			return false;
		}
		return true;
	}

	// Decode an image to raw RGBA via WIC. The primary path handles anything
	// WIC decodes from memory - DDS (DXT1/3/5 and DX10-header formats), PNG,
	// JPEG - with a manual fallback for legacy uncompressed 32bpp DDS files
	// the codec rejects.
	bool DecodeImageToRGBA(const std::vector<std::uint8_t>& dds, std::vector<std::uint8_t>& rgba,
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
	// mod replaces). MessageBoxMenu, the fader/cursor menus, and Console
	// deliberately aren't listed - the HUD stays visible under those (the
	// Flash original kept rendering with the console open).
	constexpr const char* HIDE_MENUS[] = {
		"Dialogue Menu", "InventoryMenu", "MagicMenu", "MapMenu",
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
		// A freshly (re)created view needs one resync so consumers repopulate
		// it; if Ultralight ever re-readies the DOM, this re-arms that.
		host.viewFresh_.store(true);
		{
			// Fresh JS heap = empty view-side image cache; ship pixels anew.
			std::scoped_lock lock(host.mtx_);
			host.pushedImages_.clear();
		}
		// Flip domReady_ only once the queue is observed empty under the
		// lock. A Send racing this drain still lands in pending_ (ready is
		// false) and is picked up by the next pass - nothing can dispatch
		// ahead of ops queued before it, so op order is preserved across
		// the ready transition.
		for (;;) {
			std::vector<std::string> backlog;
			{
				std::scoped_lock lock(host.mtx_);
				if (host.pending_.empty()) {
					host.domReady_ = true;
					break;
				}
				backlog.swap(host.pending_);
			}
			for (auto& json : backlog) {
				host.EnqueueOp(std::move(json));
			}
		}
	});

	api_->RegisterJSListener(view_, "iwMetrics", [](const char* arg) {
		int id = 0, w = 0, h = 0;
		if (arg && sscanf_s(arg, R"({"id":%d,"w":%d,"h":%d})", &id, &w, &h) == 3) {
			WidgetHost::Get().SetMetrics(id, w, h);
		}
	});

	// DIAG: v1-safe view->DLL channel for the hidden-icon anomaly probe.
	api_->RegisterJSListener(view_, "iwDiag", [](const char* arg) {
		if (arg) {
			logger::info("VIEWDIAG: {}", arg);
		}
	});

	// DIAG: capture the view's own console.log into our log (needs PrismaUI's
	// v2 interface; nullptr if the installed PrismaUI is too old to support it).
	if (auto* api2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>()) {
		api2->RegisterConsoleCallback(view_,
			[](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
				if (msg) {
					logger::info("VIEW: {}", msg);
				}
			});
		logger::info("DIAG: view console callback registered (v2)");
	} else {
		logger::info("DIAG: PrismaUI v2 unavailable - no view console capture");
	}

	if (auto* ui = RE::UI::GetSingleton()) {
		ui->AddEventSink(MenuWatcher::GetSingleton());
	}

	// There is no event for the global menus-shown flag (`tm`, and native
	// HUD-hiders like SexLab's Hide HUD flip it directly), so poll it. The
	// stop token keeps the thread from touching engine singletons during
	// teardown: the jthread joins when the host is destroyed, waiting at
	// most one poll interval.
	hudPoll_ = std::jthread([](std::stop_token st) {
		while (!st.stop_requested()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			if (st.stop_requested()) {
				break;
			}
			if (auto* ui = RE::UI::GetSingleton()) {
				WidgetHost::Get().SetGameHudShown(ui->IsShowingMenus());
			}
		}
	});

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
	EnqueueOp(std::move(json));
}

void WidgetHost::EnqueueOp(std::string json)
{
	bool schedule = false;
	{
		std::scoped_lock lock(mtx_);
		batch_.push_back(std::move(json));
		if (!flushQueued_) {
			flushQueued_ = true;
			schedule = true;
		}
	}
	// Serialize all view access onto the main thread; natives run on Papyrus
	// VM threads and Ultralight is not documented thread-safe. One task per
	// burst: everything queued before the task runs goes out as one batch.
	if (schedule) {
		SKSE::GetTaskInterface()->AddTask([]() { WidgetHost::Get().FlushBatch(); });
	}
}

void WidgetHost::FlushBatch()
{
	std::vector<std::string> ops;
	{
		std::scoped_lock lock(mtx_);
		ops.swap(batch_);
		flushQueued_ = false;
	}
	if (ops.empty() || !api_ || !view_ || !api_->IsValid(view_)) {
		return;
	}
	std::size_t total = 2;
	for (const auto& op : ops) {
		total += op.size() + 1;
	}
	std::string arr;
	arr.reserve(total);
	arr += '[';
	for (std::size_t i = 0; i < ops.size(); ++i) {
		if (i) {
			arr += ',';
		}
		arr += ops[i];
	}
	arr += ']';
	api_->InteropCall(view_, "iwCall", arr.c_str());
}

bool WidgetHost::ShouldSendPixels(const std::string& key)
{
	std::scoped_lock lock(mtx_);
	return pushedImages_.insert(key).second;
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

	// Format-agnostic: decode whatever the consumer asked for by its actual
	// extension. Which file/format to request is the CONSUMER's decision
	// (e.g. SL Widgets), not the renderer's - the DLL never guesses extensions
	// or scans folders. A `*_<N>f[@ms].<ext>` name is sliced as a spritesheet.
	ImageData data;
	const std::string relPath = NormalizePath(file);
	const std::string ext = ToLower(std::filesystem::path(relPath).extension().string());

	std::vector<std::uint8_t> bytes;
	if (!ReadGameFile(relPath, bytes)) {
		logger::error("loadWidget: cannot read '{}' (loose or BSA)", relPath);
	} else if (ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
			   ext == ".gif") {
		if (ext == ".gif" && DecodeGifFrames(bytes, data)) {
			// Animated GIF: composited frames carry it from here.
		} else {
			std::vector<std::uint8_t> rgba;
			if (DecodeImageToRGBA(bytes, rgba, data.w, data.h)) {
				if (!SliceSpritesheet(file, rgba, data.w, data.h, data)) {
					data.px = Base64(rgba.data(), rgba.size());
				}
			} else if (ext != ".dds") {
				// Last resort: hand the encoded file to the view's own loader.
				const char* mime = (ext == ".png") ? "image/png" :
								   (ext == ".gif") ? "image/gif" : "image/jpeg";
				data.url = std::format("data:{};base64,{}", mime,
					Base64(bytes.data(), bytes.size()));
			} else {
				logger::error("loadWidget: decode failed for '{}'", relPath);
			}
		}
	} else {
		// .swf widgets cannot be rendered outside Scaleform.
		logger::error("loadWidget: unsupported format '{}' for '{}'", ext, relPath);
	}

	if (data.px.empty() && data.url.empty() && data.frames.empty()) {
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

void WidgetHost::EraseMetrics(int id)
{
	std::scoped_lock lock(mtx_);
	metrics_.erase(id);
}

void WidgetHost::ClearAllMetrics()
{
	std::scoped_lock lock(mtx_);
	metrics_.clear();
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

	Obj& Obj::Raw(std::string_view key, std::string_view rawJson)
	{
		Comma();
		body_ += std::format(R"("{}":{})", key, rawJson);
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

namespace Text
{
	namespace
	{
		// Structural validity only (lead/continuation byte shapes) - enough to
		// distinguish UTF-8 from single-byte ANSI text, where any byte >= 0x80
		// is a lone lead byte with no continuation.
		bool IsValidUtf8(std::string_view s)
		{
			std::size_t i = 0;
			while (i < s.size()) {
				const auto c = static_cast<unsigned char>(s[i]);
				std::size_t len;
				if (c < 0x80) {
					len = 1;
				} else if ((c & 0xE0) == 0xC0) {
					len = 2;
				} else if ((c & 0xF0) == 0xE0) {
					len = 3;
				} else if ((c & 0xF8) == 0xF0) {
					len = 4;
				} else {
					return false;
				}
				if (i + len > s.size()) {
					return false;
				}
				for (std::size_t j = 1; j < len; ++j) {
					if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) {
						return false;
					}
				}
				i += len;
			}
			return true;
		}
	}

	std::string ToUtf8(std::string s)
	{
		if (s.empty() || IsValidUtf8(s)) {
			return s;
		}
		// CP_ACP is a best-effort guess (right for text matching the system
		// locale, e.g. cp1251 strings on a Russian system); on a wrong guess
		// the result is still valid UTF-8 rather than replacement glyphs.
		const int wlen = MultiByteToWideChar(CP_ACP, 0, s.data(),
			static_cast<int>(s.size()), nullptr, 0);
		if (wlen <= 0) {
			return s;
		}
		std::wstring wide(static_cast<std::size_t>(wlen), L'\0');
		MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()),
			wide.data(), wlen);
		const int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen,
			nullptr, 0, nullptr, nullptr);
		if (ulen <= 0) {
			return s;
		}
		std::string out(static_cast<std::size_t>(ulen), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, out.data(), ulen,
			nullptr, nullptr);
		return out;
	}
}
