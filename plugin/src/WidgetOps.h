#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The widget operations, one definition each, called by the Papyrus natives
// backing iWantWidgetsNative.psc. Semantics live here (id allocation, the
// image pixel cache, the size cache feeding getXsize/getYsize, colour
// packing) so the natives stay pure argument marshalling. Everything here is
// safe to call from any thread.
namespace WidgetOps
{
	int  LoadWidget(const std::string& file, int x, int y, bool visible);
	int  LoadText(std::string text, std::string font, int size, int x, int y, bool visible);
	int  LoadMeter(int x, int y, bool visible);

	void SetText(int id, std::string text);
	void AppendText(int id, std::string text);

	void SetPos(int id, int x, int y);
	void SetSize(int id, int h, int w);

	int  GetXSize(int id);
	int  GetYSize(int id);

	void SetZoom(int id, int xscale, int yscale);

	void SetVisible(int id, bool visible);
	void SetRotation(int id, int rotation);
	void SetTransparency(int id, int alpha);

	void SetRGB(int id, int r, int g, int b);

	void SendToBack(int id);
	void SendToFront(int id);
	void SwapDepths(int id1, int id2);
	void Destroy(int id);
	void SetAllVisible(bool visible);

	void DrawShapeLine(const std::vector<std::int32_t>& list, int x, int y, int dx, int dy,
		bool skipInvisible, bool skipAlpha0);
	void DrawShapeCircle(const std::vector<std::int32_t>& list, int x, int y, int radius,
		int startAngle, int degreeChange, bool skipInvisible, bool skipAlpha0, bool autoSpace);
	void DrawShapeOrbit(const std::vector<std::int32_t>& list, int x, int y, int radius,
		int startAngle, int degreeChange, bool skipInvisible, bool skipAlpha0, bool autoSpace);

	// attr/easing arrive already normalized ("_alpha", "_x", "percent";
	// "regular"/"none"; "easeIn"/"easeOut"/"easeInOut"/"") - the mapping
	// lives in iwant_widgets.psc.
	void DoTransition(int id, float target, float seconds, const std::string& attr,
		const std::string& easingClass, const std::string& easingMethod, float delay);

	void SetMeterPercent(int id, int percent);
	void SetMeterFillDirection(int id, const std::string& direction);
	void DoMeterFlash(int id);
	void SetMeterColors(int id, int light, int dark, int flash);

	void Reset();
}
