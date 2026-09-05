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

	// Mirrors the vanilla HUD's visibility: the Flash original lived inside
	// the HUD menu and was hidden with it. The Prisma overlay has to follow
	// two signals explicitly: HUD-suppressing menus (dialogue, inventory,
	// loading...) and the global menus-shown flag (the `tm` toggle, which is
	// also how native HUD-hiders like SexLab's Hide HUD work).
	void SetMenusClear(bool clear);
	void SetGameHudShown(bool shown);

	int NextId();
	int PeekNextId() const;

	// Queue one JSON op for the view. Ordered; safe from any Papyrus thread.
	void Send(std::string json);

	struct ImageData
	{
		// Images are decoded to raw RGBA and shipped as base64 pixels (px) —
		// the view paints them on a canvas, bypassing Ultralight's image
		// loader and CSS-mask support entirely. Encoded data: URLs remain
		// only as a last-resort fallback.
		//
		// Animated icons carry `frames` instead of `px`: composited GIF
		// frames, or slices of a spritesheet named `*_<N>f[@<ms>].<ext>`.
		struct Frame
		{
			int ms = 100;
			std::string px;
		};
		std::string px;
		std::string url;
		std::vector<Frame> frames;
		int w = 0;
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

	// Find the actual asset for a requested path: exact, then alternate
	// extension, then a `<base>_<N>f` spritesheet in the folder. Fills bytes +
	// the winning filename (which drives spritesheet slicing). See the .cpp.
	bool ResolveAsset(const std::string& relPath, std::vector<std::uint8_t>& bytes,
		std::string& resolvedName);

	void ApplyVisibility();

	PRISMA_UI_API::IVPrismaUI1* api_ = nullptr;
	PrismaView view_ = 0;
	std::atomic<bool> domReady_{ false };
	std::atomic<bool> overlayVisible_{ true };
	std::atomic<bool> menusClear_{ true };
	std::atomic<bool> gameHudShown_{ true };
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
		Obj& Raw(std::string_view key, std::string_view rawJson);
		std::string Build();

	private:
		std::string body_;
		void Comma();
	};
}
