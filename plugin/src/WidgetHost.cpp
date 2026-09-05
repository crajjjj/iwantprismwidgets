#include "WidgetHost.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include <DirectXTex.h>
#include <objbase.h>

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
		return std::string(ROOT_PREFIX) + p;
	}

	std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	// Reads through the game's resource system, so both loose files and BSA
	// archives resolve. Path is relative to Data/.
	bool ReadGameFile(const std::string& relPath, std::vector<std::uint8_t>& out)
	{
		RE::BSResourceNiBinaryStream stream(relPath);
		if (!stream.good()) {
			return false;
		}
		constexpr std::size_t CHUNK = 64 * 1024;
		std::uint8_t buf[CHUNK];
		std::size_t got = 0;
		do {
			got = stream.read(buf, CHUNK);
			out.insert(out.end(), buf, buf + got);
		} while (got == CHUNK);
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
		// WIC (used by DirectXTex for PNG encode) needs COM on the calling
		// thread; natives arrive on arbitrary Papyrus VM threads.
		thread_local bool initialized = [] {
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			return true;
		}();
		(void)initialized;
	}

	bool DecodeDDSToPNG(const std::vector<std::uint8_t>& dds, std::vector<std::uint8_t>& png,
		int& w, int& h)
	{
		EnsureCom();

		DirectX::TexMetadata meta{};
		DirectX::ScratchImage image;
		if (FAILED(DirectX::LoadFromDDSMemory(dds.data(), dds.size(),
				DirectX::DDS_FLAGS_NONE, &meta, image))) {
			return false;
		}

		DirectX::ScratchImage converted;
		const DirectX::Image* src = image.GetImage(0, 0, 0);
		if (!src) {
			return false;
		}

		if (DirectX::IsCompressed(meta.format)) {
			if (FAILED(DirectX::Decompress(*src, DXGI_FORMAT_R8G8B8A8_UNORM, converted))) {
				return false;
			}
			src = converted.GetImage(0, 0, 0);
		} else if (meta.format != DXGI_FORMAT_R8G8B8A8_UNORM &&
				   meta.format != DXGI_FORMAT_B8G8R8A8_UNORM) {
			if (FAILED(DirectX::Convert(*src, DXGI_FORMAT_R8G8B8A8_UNORM,
					DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted))) {
				return false;
			}
			src = converted.GetImage(0, 0, 0);
		}

		DirectX::Blob blob;
		if (FAILED(DirectX::SaveToWICMemory(*src, DirectX::WIC_FLAGS_NONE,
				DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), blob))) {
			return false;
		}

		const auto* p = static_cast<const std::uint8_t*>(blob.GetBufferPointer());
		png.assign(p, p + blob.GetBufferSize());
		w = static_cast<int>(src->width);
		h = static_cast<int>(src->height);
		return true;
	}
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
		if (arg && std::sscanf(arg, R"({"id":%d,"w":%d,"h":%d})", &id, &w, &h) == 3) {
			WidgetHost::Get().SetMetrics(id, w, h);
		}
	});

	logger::info("iWant Widgets view created ({})", VIEW_PATH);
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
		std::vector<std::uint8_t> png;
		if (DecodeDDSToPNG(bytes, png, data.w, data.h)) {
			data.url = "data:image/png;base64," + Base64(png.data(), png.size());
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

	if (data.url.empty()) {
		// 1x1 transparent PNG so a bad path degrades to an invisible widget
		// instead of a broken-image glyph.
		data.url =
			"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJ"
			"AAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==";
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
