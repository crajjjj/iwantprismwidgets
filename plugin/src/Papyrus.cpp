#include "WidgetHost.h"

// Papyrus natives backing iWantWidgetsNative.psc. Thin: allocate ids, build
// the JSON op, hand it to WidgetHost. All widget semantics live in the view.

namespace
{
	using VM = RE::BSScript::IVirtualMachine;
	using Tag = RE::StaticFunctionTag;

	constexpr const char* SCRIPT = "iWantWidgetsNative";

	bool IsReady(Tag*)
	{
		return WidgetHost::Get().IsReady();
	}

	std::int32_t LoadWidget(Tag*, std::string file, std::int32_t x, std::int32_t y, bool visible)
	{
		auto& host = WidgetHost::Get();
		const auto& img = host.LoadImageFile(file);
		const int id = host.NextId();
		host.SetMetrics(id, img.w, img.h);
		logger::info("DIAG loadWidget: id={} file={}", id, file);
		Json::Obj o;
		o.Str("op", "loadWidget").Int("id", id);
		if (!img.frames.empty() || !img.px.empty()) {
			// Consumers reload the same icon files over and over (every MCM
			// toggle rebuilds whole bars). The base64 pixels dominate the op
			// payload, so ship them only the first time; after that the op
			// names the file and the view reuses its cached decode.
			std::string key = file;
			for (auto& c : key) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			o.Str("file", key);
			if (host.ShouldSendPixels(key)) {
				if (!img.frames.empty()) {
					std::string arr = "[";
					for (std::size_t i = 0; i < img.frames.size(); ++i) {
						if (i) {
							arr += ',';
						}
						arr += std::format(R"({{"ms":{},"px":"{}"}})", img.frames[i].ms,
							img.frames[i].px);
					}
					arr += ']';
					o.Raw("frames", arr);
				} else {
					o.Str("px", img.px);
				}
			}
		} else {
			o.Str("url", img.url);
		}
		o.Int("w", img.w)
			.Int("h", img.h)
			.Int("x", x)
			.Int("y", y)
			.Boolean("vis", visible);
		host.Send(o.Build());
		return id;
	}

	std::int32_t LoadText(Tag*, std::string text, std::string font, std::int32_t size,
		std::int32_t x, std::int32_t y, bool visible)
	{
		auto& host = WidgetHost::Get();
		const int id = host.NextId();
		host.Send(Json::Obj()
				.Str("op", "loadText")
				.Int("id", id)
				.Str("text", Text::ToUtf8(std::move(text)))
				.Str("font", Text::ToUtf8(std::move(font)))
				.Int("size", size)
				.Int("x", x)
				.Int("y", y)
				.Boolean("vis", visible)
				.Build());
		return id;
	}

	std::int32_t LoadMeter(Tag*, std::int32_t x, std::int32_t y, bool visible)
	{
		auto& host = WidgetHost::Get();
		const int id = host.NextId();
		host.SetMetrics(id, 334, 30);
		host.Send(Json::Obj()
				.Str("op", "loadMeter")
				.Int("id", id)
				.Int("x", x)
				.Int("y", y)
				.Boolean("vis", visible)
				.Build());
		return id;
	}

	void SetText(Tag*, std::int32_t id, std::string text)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "setText")
				.Int("id", id)
				.Str("text", Text::ToUtf8(std::move(text)))
				.Build());
	}

	void AppendText(Tag*, std::int32_t id, std::string text)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "appendText")
				.Int("id", id)
				.Str("text", Text::ToUtf8(std::move(text)))
				.Build());
	}

	void SetPos(Tag*, std::int32_t id, std::int32_t x, std::int32_t y)
	{
		// DIAG: moving a widget off-stage is how _moveIconOffscreen hides an
		// icon whose _findBarOfIcon returned -1. Log it to catch vanishings.
		if (x >= 9000 || y >= 9000) {
			logger::info("DIAG offscreen: setPos id={} -> {},{}", id, x, y);
		}
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setPos").Int("id", id).Int("x", x).Int("y", y).Build());
	}

	void SetSize(Tag*, std::int32_t id, std::int32_t h, std::int32_t w)
	{
		logger::info("DIAG setSize: id={} {}x{}", id, w, h);
		WidgetHost::Get().SetMetrics(id, w, h);
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setSize").Int("id", id).Int("w", w).Int("h", h).Build());
	}

	std::int32_t GetXSize(Tag*, std::int32_t id)
	{
		return WidgetHost::Get().GetMetrics(id).first;
	}

	std::int32_t GetYSize(Tag*, std::int32_t id)
	{
		return WidgetHost::Get().GetMetrics(id).second;
	}

	void SetZoom(Tag*, std::int32_t id, std::int32_t xs, std::int32_t ys)
	{
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setZoom").Int("id", id).Int("xs", xs).Int("ys", ys).Build());
	}

	void SetVisible(Tag*, std::int32_t id, std::int32_t visible)
	{
		if (visible == 0) {
			logger::info("DIAG hide: setVisible id={} -> false", id);
		}
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setVisible").Int("id", id).Boolean("vis", visible != 0).Build());
	}

	void SetRotation(Tag*, std::int32_t id, std::int32_t rot)
	{
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setRotation").Int("id", id).Int("rot", rot).Build());
	}

	void SetTransparency(Tag*, std::int32_t id, std::int32_t a)
	{
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setAlpha").Int("id", id).Int("a", a).Build());
	}

	void SetRGB(Tag*, std::int32_t id, std::int32_t r, std::int32_t g, std::int32_t b)
	{
		logger::info("DIAG setRGB: id={} rgb={},{},{}", id, r, g, b);
		const std::int32_t rgb = (r << 16) | (g << 8) | b;
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setColor").Int("id", id).Int("rgb", rgb).Build());
	}

	void SendToBack(Tag*, std::int32_t id)
	{
		WidgetHost::Get().Send(Json::Obj().Str("op", "toBack").Int("id", id).Build());
	}

	void SendToFront(Tag*, std::int32_t id)
	{
		WidgetHost::Get().Send(Json::Obj().Str("op", "toFront").Int("id", id).Build());
	}

	void SwapDepths(Tag*, std::int32_t id1, std::int32_t id2)
	{
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "swapDepths").Int("id1", id1).Int("id2", id2).Build());
	}

	void Destroy(Tag*, std::int32_t id)
	{
		logger::info("DIAG destroy: id={}", id);
		WidgetHost::Get().EraseMetrics(id);
		WidgetHost::Get().Send(Json::Obj().Str("op", "destroy").Int("id", id).Build());
	}

	void SetAllVisible(Tag*, bool visible)
	{
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "setAllVisible").Boolean("vis", visible).Build());
	}

	void DrawShapeLine(Tag*, std::vector<std::int32_t> list, std::int32_t x, std::int32_t y,
		std::int32_t dx, std::int32_t dy, bool skipInvisible, bool skipAlpha0)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "drawLine")
				.IntArray("list", list)
				.Int("x", x)
				.Int("y", y)
				.Int("dx", dx)
				.Int("dy", dy)
				.Boolean("skipInv", skipInvisible)
				.Boolean("skipA0", skipAlpha0)
				.Build());
	}

	void DrawShapeCircle(Tag*, std::vector<std::int32_t> list, std::int32_t x, std::int32_t y,
		std::int32_t radius, std::int32_t startAngle, std::int32_t degreeChange,
		bool skipInvisible, bool skipAlpha0, bool autoSpace)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "drawCircle")
				.IntArray("list", list)
				.Int("x", x)
				.Int("y", y)
				.Int("radius", radius)
				.Int("startAngle", startAngle)
				.Int("degreeChange", degreeChange)
				.Boolean("skipInv", skipInvisible)
				.Boolean("skipA0", skipAlpha0)
				.Boolean("autoSpace", autoSpace)
				.Build());
	}

	void DrawShapeOrbit(Tag*, std::vector<std::int32_t> list, std::int32_t x, std::int32_t y,
		std::int32_t radius, std::int32_t startAngle, std::int32_t degreeChange,
		bool skipInvisible, bool skipAlpha0, bool autoSpace)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "drawOrbit")
				.IntArray("list", list)
				.Int("x", x)
				.Int("y", y)
				.Int("radius", radius)
				.Int("startAngle", startAngle)
				.Int("degreeChange", degreeChange)
				.Boolean("skipInv", skipInvisible)
				.Boolean("skipA0", skipAlpha0)
				.Boolean("autoSpace", autoSpace)
				.Build());
	}

	void DoTransition(Tag*, std::int32_t id, float target, float seconds, std::string attr,
		std::string easingClass, std::string easingMethod, float delay)
	{
		if (attr == "_alpha") {
			logger::info("DIAG doTransition alpha: id={} -> {} ({}s)", id, target, seconds);
		}
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "doTransition")
				.Int("id", id)
				.Num("target", target)
				.Num("seconds", seconds)
				.Str("attr", attr)
				.Str("easeClass", easingClass)
				.Str("easeMethod", easingMethod)
				.Num("delay", delay)
				.Build());
	}

	void SetMeterPercent(Tag*, std::int32_t id, std::int32_t percent)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "meterPercent")
				.Int("id", id)
				.Num("pct", percent / 100.0)
				.Build());
	}

	void SetMeterFillDirection(Tag*, std::int32_t id, std::string direction)
	{
		WidgetHost::Get().Send(
			Json::Obj().Str("op", "meterDir").Int("id", id).Str("dir", direction).Build());
	}

	void DoMeterFlash(Tag*, std::int32_t id)
	{
		WidgetHost::Get().Send(Json::Obj().Str("op", "meterFlash").Int("id", id).Build());
	}

	void SetMeterColors(Tag*, std::int32_t id, std::int32_t light, std::int32_t dark,
		std::int32_t flash)
	{
		WidgetHost::Get().Send(Json::Obj()
				.Str("op", "meterColors")
				.Int("id", id)
				.Int("light", light)
				.Int("dark", dark)
				.Int("flash", flash)
				.Build());
	}

	bool NeedsResync(Tag*)
	{
		return WidgetHost::Get().NeedsResync();
	}

	void Reset(Tag*)
	{
		auto& host = WidgetHost::Get();
		host.MarkResynced();
		// Ids are never reused, so every cached size belongs to a dead widget.
		host.ClearAllMetrics();
		host.Send(Json::Obj()
				.Str("op", "reset")
				.Int("nextId", host.PeekNextId())
				.Build());
	}
}

bool RegisterPapyrusFunctions(VM* vm)
{
	// callableFromTasklets = true: the VM runs these directly on its stack
	// threads instead of synchronizing each call to the main thread. All of
	// them only touch WidgetHost (mutexes, atomics, task-queued view access),
	// which was designed for exactly that - and it keeps first-time image
	// decodes in LoadWidget off the render frame.
	vm->RegisterFunction("IsReady", SCRIPT, IsReady, true);
	vm->RegisterFunction("LoadWidget", SCRIPT, LoadWidget, true);
	vm->RegisterFunction("LoadText", SCRIPT, LoadText, true);
	vm->RegisterFunction("LoadMeter", SCRIPT, LoadMeter, true);
	vm->RegisterFunction("SetText", SCRIPT, SetText, true);
	vm->RegisterFunction("AppendText", SCRIPT, AppendText, true);
	vm->RegisterFunction("SetPos", SCRIPT, SetPos, true);
	vm->RegisterFunction("SetSize", SCRIPT, SetSize, true);
	vm->RegisterFunction("GetXSize", SCRIPT, GetXSize, true);
	vm->RegisterFunction("GetYSize", SCRIPT, GetYSize, true);
	vm->RegisterFunction("SetZoom", SCRIPT, SetZoom, true);
	vm->RegisterFunction("SetVisible", SCRIPT, SetVisible, true);
	vm->RegisterFunction("SetRotation", SCRIPT, SetRotation, true);
	vm->RegisterFunction("SetTransparency", SCRIPT, SetTransparency, true);
	vm->RegisterFunction("SetRGB", SCRIPT, SetRGB, true);
	vm->RegisterFunction("SendToBack", SCRIPT, SendToBack, true);
	vm->RegisterFunction("SendToFront", SCRIPT, SendToFront, true);
	vm->RegisterFunction("SwapDepths", SCRIPT, SwapDepths, true);
	vm->RegisterFunction("Destroy", SCRIPT, Destroy, true);
	vm->RegisterFunction("SetAllVisible", SCRIPT, SetAllVisible, true);
	vm->RegisterFunction("DrawShapeLine", SCRIPT, DrawShapeLine, true);
	vm->RegisterFunction("DrawShapeCircle", SCRIPT, DrawShapeCircle, true);
	vm->RegisterFunction("DrawShapeOrbit", SCRIPT, DrawShapeOrbit, true);
	vm->RegisterFunction("DoTransition", SCRIPT, DoTransition, true);
	vm->RegisterFunction("SetMeterPercent", SCRIPT, SetMeterPercent, true);
	vm->RegisterFunction("SetMeterFillDirection", SCRIPT, SetMeterFillDirection, true);
	vm->RegisterFunction("DoMeterFlash", SCRIPT, DoMeterFlash, true);
	vm->RegisterFunction("SetMeterColors", SCRIPT, SetMeterColors, true);
	vm->RegisterFunction("Reset", SCRIPT, Reset, true);
	vm->RegisterFunction("NeedsResync", SCRIPT, NeedsResync, true);
	return true;
}
