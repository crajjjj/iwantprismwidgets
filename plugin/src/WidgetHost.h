#pragma once

#include "PrismaUI_API.h"

// Owns the PrismaUI view and everything shared between the Papyrus natives and
// the JS renderer: the widget id counter, the ordered call pipeline (queued
// until the view DOM is ready), the size cache fed back from JS, and the
// image decode cache.
class WidgetHost
{
public:
	static WidgetHost& Get();

	// kDataLoaded: acquire the PrismaUI API, create the view, hook listeners.
	void OnDataLoaded();

	bool IsReady() const;

	int NextId();
	int PeekNextId() const;

	// Queue one JSON op for the view. Ordered; safe from any Papyrus thread.
	void Send(std::string json);

	struct ImageData
	{
		std::string url;  // data: URL (PNG for decoded DDS, passthrough otherwise)
		int w = 0;        // pixel size when known at decode time (DDS)
		int h = 0;
	};

	// file is the raw path consumers pass to loadWidget, rooted (like the
	// Flash original) at Data/Interface/exported/. Reads loose files and BSA
	// contents; decodes DDS (incl. BC-compressed) to PNG. Cached per path.
	const ImageData& LoadImageFile(const std::string& file);

	void SetMetrics(int id, int w, int h);
	std::pair<int, int> GetMetrics(int id) const;

private:
	WidgetHost() = default;

	void Dispatch(const std::string& json);  // task-queued InteropCall

	PRISMA_UI_API::IVPrismaUI1* api_ = nullptr;
	PrismaView view_ = 0;
	std::atomic<bool> domReady_{ false };
	std::atomic<int> nextId_{ 1 };

	mutable std::mutex mtx_;
	std::vector<std::string> pending_;
	std::unordered_map<int, std::pair<int, int>> metrics_;
	std::unordered_map<std::string, ImageData> imageCache_;
};

namespace Json
{
	std::string Escape(std::string_view s);

	// Tiny builder for the flat payloads this plugin sends.
	class Obj
	{
	public:
		Obj& Str(std::string_view key, std::string_view value);
		Obj& Num(std::string_view key, double value);
		Obj& Int(std::string_view key, std::int64_t value);
		Obj& Boolean(std::string_view key, bool value);
		Obj& IntArray(std::string_view key, const std::vector<std::int32_t>& values);
		std::string Build();

	private:
		std::string body_;
		void Comma();
	};
}
