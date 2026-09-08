#include "WidgetHost.h"
#include "WidgetOps.h"

// Papyrus natives backing iWantWidgetsNative.psc. Nothing but argument
// marshalling: every widget semantic lives in WidgetOps and, below that, in
// the view.

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
		return WidgetOps::LoadWidget(file, x, y, visible);
	}

	std::int32_t LoadText(Tag*, std::string text, std::string font, std::int32_t size,
		std::int32_t x, std::int32_t y, bool visible)
	{
		return WidgetOps::LoadText(std::move(text), std::move(font), size, x, y, visible);
	}

	std::int32_t LoadMeter(Tag*, std::int32_t x, std::int32_t y, bool visible)
	{
		return WidgetOps::LoadMeter(x, y, visible);
	}

	void SetText(Tag*, std::int32_t id, std::string text)
	{
		WidgetOps::SetText(id, std::move(text));
	}

	void AppendText(Tag*, std::int32_t id, std::string text)
	{
		WidgetOps::AppendText(id, std::move(text));
	}

	void SetPos(Tag*, std::int32_t id, std::int32_t x, std::int32_t y)
	{
		WidgetOps::SetPos(id, x, y);
	}

	void SetSize(Tag*, std::int32_t id, std::int32_t h, std::int32_t w)
	{
		WidgetOps::SetSize(id, h, w);
	}

	std::int32_t GetXSize(Tag*, std::int32_t id)
	{
		return WidgetOps::GetXSize(id);
	}

	std::int32_t GetYSize(Tag*, std::int32_t id)
	{
		return WidgetOps::GetYSize(id);
	}

	void SetZoom(Tag*, std::int32_t id, std::int32_t xs, std::int32_t ys)
	{
		WidgetOps::SetZoom(id, xs, ys);
	}

	void SetVisible(Tag*, std::int32_t id, std::int32_t visible)
	{
		WidgetOps::SetVisible(id, visible != 0);
	}

	void SetRotation(Tag*, std::int32_t id, std::int32_t rot)
	{
		WidgetOps::SetRotation(id, rot);
	}

	void SetTransparency(Tag*, std::int32_t id, std::int32_t a)
	{
		WidgetOps::SetTransparency(id, a);
	}

	void SetRGB(Tag*, std::int32_t id, std::int32_t r, std::int32_t g, std::int32_t b)
	{
		WidgetOps::SetRGB(id, r, g, b);
	}

	void SendToBack(Tag*, std::int32_t id)
	{
		WidgetOps::SendToBack(id);
	}

	void SendToFront(Tag*, std::int32_t id)
	{
		WidgetOps::SendToFront(id);
	}

	void SwapDepths(Tag*, std::int32_t id1, std::int32_t id2)
	{
		WidgetOps::SwapDepths(id1, id2);
	}

	void Destroy(Tag*, std::int32_t id)
	{
		WidgetOps::Destroy(id);
	}

	void SetAllVisible(Tag*, bool visible)
	{
		WidgetOps::SetAllVisible(visible);
	}

	void DrawShapeLine(Tag*, std::vector<std::int32_t> list, std::int32_t x, std::int32_t y,
		std::int32_t dx, std::int32_t dy, bool skipInvisible, bool skipAlpha0)
	{
		WidgetOps::DrawShapeLine(list, x, y, dx, dy, skipInvisible, skipAlpha0);
	}

	void DrawShapeCircle(Tag*, std::vector<std::int32_t> list, std::int32_t x, std::int32_t y,
		std::int32_t radius, std::int32_t startAngle, std::int32_t degreeChange,
		bool skipInvisible, bool skipAlpha0, bool autoSpace)
	{
		WidgetOps::DrawShapeCircle(list, x, y, radius, startAngle, degreeChange, skipInvisible,
			skipAlpha0, autoSpace);
	}

	void DrawShapeOrbit(Tag*, std::vector<std::int32_t> list, std::int32_t x, std::int32_t y,
		std::int32_t radius, std::int32_t startAngle, std::int32_t degreeChange,
		bool skipInvisible, bool skipAlpha0, bool autoSpace)
	{
		WidgetOps::DrawShapeOrbit(list, x, y, radius, startAngle, degreeChange, skipInvisible,
			skipAlpha0, autoSpace);
	}

	void DoTransition(Tag*, std::int32_t id, float target, float seconds, std::string attr,
		std::string easingClass, std::string easingMethod, float delay)
	{
		WidgetOps::DoTransition(id, target, seconds, attr, easingClass, easingMethod, delay);
	}

	void SetMeterPercent(Tag*, std::int32_t id, std::int32_t percent)
	{
		WidgetOps::SetMeterPercent(id, percent);
	}

	void SetMeterFillDirection(Tag*, std::int32_t id, std::string direction)
	{
		WidgetOps::SetMeterFillDirection(id, direction);
	}

	void DoMeterFlash(Tag*, std::int32_t id)
	{
		WidgetOps::DoMeterFlash(id);
	}

	void SetMeterColors(Tag*, std::int32_t id, std::int32_t light, std::int32_t dark,
		std::int32_t flash)
	{
		WidgetOps::SetMeterColors(id, light, dark, flash);
	}

	bool NeedsResync(Tag*)
	{
		// The alias calls this on every game load before anything else, which
		// makes it the one place guaranteed to run once our scripts are live.
		WidgetHost::Get().CheckScriptBinding();
		return WidgetHost::Get().NeedsResync();
	}

	void Reset(Tag*)
	{
		WidgetOps::Reset();
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
