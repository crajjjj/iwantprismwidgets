#include "WidgetOps.h"

#include "WidgetHost.h"

#include <algorithm>
#include <cctype>

namespace
{
	void Send(std::string json)
	{
		WidgetHost::Get().Send(std::move(json));
	}
}

namespace WidgetOps
{
	int LoadWidget(const std::string& file, int x, int y, bool visible)
	{
		auto& host = WidgetHost::Get();
		const auto& img = host.LoadImageFile(file);
		const int id = host.NextId();
		host.SetMetrics(id, img.w, img.h);
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
			if (!img.tint) {
				o.Boolean("tint", false);
			}
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
		Send(o.Build());
		return id;
	}

	int LoadText(std::string text, std::string font, int size, int x, int y, bool visible)
	{
		const int id = WidgetHost::Get().NextId();
		Send(Json::Obj()
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

	int LoadMeter(int x, int y, bool visible)
	{
		auto& host = WidgetHost::Get();
		const int id = host.NextId();
		host.SetMetrics(id, 334, 30);
		Send(Json::Obj()
				.Str("op", "loadMeter")
				.Int("id", id)
				.Int("x", x)
				.Int("y", y)
				.Boolean("vis", visible)
				.Build());
		return id;
	}

	void SetText(int id, std::string text)
	{
		Send(Json::Obj()
				.Str("op", "setText")
				.Int("id", id)
				.Str("text", Text::ToUtf8(std::move(text)))
				.Build());
	}

	void AppendText(int id, std::string text)
	{
		Send(Json::Obj()
				.Str("op", "appendText")
				.Int("id", id)
				.Str("text", Text::ToUtf8(std::move(text)))
				.Build());
	}

	void SetPos(int id, int x, int y)
	{
		Send(Json::Obj().Str("op", "setPos").Int("id", id).Int("x", x).Int("y", y).Build());
	}

	void SetSize(int id, int h, int w)
	{
		WidgetHost::Get().SetMetrics(id, w, h);
		Send(Json::Obj().Str("op", "setSize").Int("id", id).Int("w", w).Int("h", h).Build());
	}

	int GetXSize(int id)
	{
		return WidgetHost::Get().GetMetrics(id).first;
	}

	int GetYSize(int id)
	{
		return WidgetHost::Get().GetMetrics(id).second;
	}

	void SetZoom(int id, int xscale, int yscale)
	{
		Send(Json::Obj()
				.Str("op", "setZoom")
				.Int("id", id)
				.Int("xs", xscale)
				.Int("ys", yscale)
				.Build());
	}

	void SetVisible(int id, bool visible)
	{
		Send(Json::Obj().Str("op", "setVisible").Int("id", id).Boolean("vis", visible).Build());
	}

	void SetRotation(int id, int rotation)
	{
		Send(Json::Obj().Str("op", "setRotation").Int("id", id).Int("rot", rotation).Build());
	}

	void SetTransparency(int id, int alpha)
	{
		Send(Json::Obj().Str("op", "setAlpha").Int("id", id).Int("a", alpha).Build());
	}

	void SetRGB(int id, int r, int g, int b)
	{
		const int rgb = (r << 16) | (g << 8) | b;
		Send(Json::Obj().Str("op", "setColor").Int("id", id).Int("rgb", rgb).Build());
	}

	void SendToBack(int id)
	{
		Send(Json::Obj().Str("op", "toBack").Int("id", id).Build());
	}

	void SendToFront(int id)
	{
		Send(Json::Obj().Str("op", "toFront").Int("id", id).Build());
	}

	void SwapDepths(int id1, int id2)
	{
		Send(Json::Obj().Str("op", "swapDepths").Int("id1", id1).Int("id2", id2).Build());
	}

	void Destroy(int id)
	{
		WidgetHost::Get().EraseMetrics(id);
		Send(Json::Obj().Str("op", "destroy").Int("id", id).Build());
	}

	void SetAllVisible(bool visible)
	{
		Send(Json::Obj().Str("op", "setAllVisible").Boolean("vis", visible).Build());
	}

	void DrawShapeLine(const std::vector<std::int32_t>& list, int x, int y, int dx, int dy,
		bool skipInvisible, bool skipAlpha0)
	{
		Send(Json::Obj()
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

	void DrawShapeCircle(const std::vector<std::int32_t>& list, int x, int y, int radius,
		int startAngle, int degreeChange, bool skipInvisible, bool skipAlpha0, bool autoSpace)
	{
		Send(Json::Obj()
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

	void DrawShapeOrbit(const std::vector<std::int32_t>& list, int x, int y, int radius,
		int startAngle, int degreeChange, bool skipInvisible, bool skipAlpha0, bool autoSpace)
	{
		Send(Json::Obj()
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

	void DoTransition(int id, float target, float seconds, const std::string& attr,
		const std::string& easingClass, const std::string& easingMethod, float delay)
	{
		Send(Json::Obj()
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

	void SetMeterPercent(int id, int percent)
	{
		Send(Json::Obj()
				.Str("op", "meterPercent")
				.Int("id", id)
				.Num("pct", percent / 100.0)
				.Build());
	}

	void SetMeterFillDirection(int id, const std::string& direction)
	{
		Send(Json::Obj().Str("op", "meterDir").Int("id", id).Str("dir", direction).Build());
	}

	void DoMeterFlash(int id)
	{
		Send(Json::Obj().Str("op", "meterFlash").Int("id", id).Build());
	}

	void SetMeterColors(int id, int light, int dark, int flash)
	{
		Send(Json::Obj()
				.Str("op", "meterColors")
				.Int("id", id)
				.Int("light", light)
				.Int("dark", dark)
				.Int("flash", flash)
				.Build());
	}

	void Reset()
	{
		auto& host = WidgetHost::Get();
		host.MarkResynced();
		// Ids are never reused, so every cached size belongs to a dead widget.
		host.ClearAllMetrics();
		Send(Json::Obj().Str("op", "reset").Int("nextId", host.PeekNextId()).Build());
	}
}
