Scriptname iWant_Widgets extends Quest
{iWant Widgets - Prisma Edition. Drop-in reimplementation of DaemonPrime's
 iWant Widgets rendering through PrismaUI (HTML/JS) instead of Scaleform/Flash.
 Public function signatures are identical to iWant Widgets 1.33 so existing
 consumers (iWant Status Bars and its addons) run without recompilation.}

; The original extended SKI_WidgetBase to piggyback on SkyUI's HUD widget
; loader. Rendering now lives in an SKSE-owned PrismaUI view, so this script
; extends Quest and SkyUI is no longer required by the widget layer.

String Property HUD_MENU = "HUD Menu" AutoReadOnly

Int Function loadWidget(String filename, Int xpos = 10000, Int ypos = 10000, Bool visible = False)
	_waitForReadyToLoad()
	Return iWantWidgetsNative.LoadWidget(filename, xpos, ypos, visible)
EndFunction

Int Function loadLibraryWidget(String filename, Int xpos = 10000, Int ypos = 10000, Bool visible = False)
	String libraryPrefix = "widgets/iwant/widgets/library/"
	String path
	Int id

	path = libraryPrefix + filename + ".dds"
	id = loadWidget(path, xpos, ypos, visible)

	Return(id)
EndFunction

Int Function loadText(String displayString, String font = "$EverywhereFont", Int size = 24, Int xpos = 10000, Int ypos = 10000, Bool visible = False)
	_waitForReadyToLoad()
	Return iWantWidgetsNative.LoadText(displayString, font, size, xpos, ypos, visible)
EndFunction

Int Function loadMeter(Int xpos = 10000, Int ypos = 10000, Bool visible = False)
	_waitForReadyToLoad()
	Return iWantWidgetsNative.LoadMeter(xpos, ypos, visible)
EndFunction

Function _waitForReadyToLoad()
	; The view is created at kDataLoaded and native calls are queued until its
	; DOM is ready, so this can only spin very early in a session. Breakout
	; mirrors the original's defensive cap.
	Int breakout = 0
	While !iWantWidgetsNative.IsReady() && breakout < 50
		Utility.Wait(0.1)
		breakout += 1
	EndWhile
EndFunction

String Function _getMessageFromFlash()
	; Flash return-value polling is obsolete: natives return directly.
	; Kept so any external caller of this quasi-internal doesn't break.
	Return ""
EndFunction

Function setMeterPercent(Int id, Int percent)
	iWantWidgetsNative.SetMeterPercent(id, percent)
EndFunction

Function setMeterFillDirection(Int id, String direction)
	iWantWidgetsNative.SetMeterFillDirection(id, direction)
EndFunction

Function sendToBack(Int id)
	iWantWidgetsNative.SendToBack(id)
EndFunction

Function sendToFront(Int id)
	iWantWidgetsNative.SendToFront(id)
EndFunction

Function doMeterFlash(Int id)
	iWantWidgetsNative.DoMeterFlash(id)
EndFunction

Function setMeterRGB(Int id, Int lightR = 255, Int lightG = 255, Int lightB = 255, Int darkR = 0, Int darkG = 0, Int darkB = 0, Int flashR = 127, Int flashG = 127, Int flashB = 127)
	Int lightRGB = lightR * 65536 + lightG * 256 + lightB
	Int darkRGB = darkR * 65536 + darkG * 256 + darkB
	Int flashRGB = flashR * 65536 + flashG * 256 + flashB
	iWantWidgetsNative.SetMeterColors(id, lightRGB, darkRGB, flashRGB)
EndFunction

Function setText(Int id, String displayString)
	iWantWidgetsNative.SetText(id, displayString)
EndFunction

Function appendText(Int id, String displayString)
	iWantWidgetsNative.AppendText(id, displayString)
EndFunction

Function swapDepths(Int id1, Int id2)
	iWantWidgetsNative.SwapDepths(id1, id2)
EndFunction

Function setPos(Int id, Int xpos, Int ypos)
	iWantWidgetsNative.SetPos(id, xpos, ypos)
EndFunction

Function setSize(Int id, Int h, Int w)
	iWantWidgetsNative.SetSize(id, h, w)
EndFunction

Int Function getXsize(Int id)
	Return iWantWidgetsNative.GetXSize(id)
EndFunction

Int Function getYsize(Int id)
	Return iWantWidgetsNative.GetYSize(id)
EndFunction

Function setZoom(Int id, Int xscale, Int yscale)
	iWantWidgetsNative.SetZoom(id, xscale, yscale)
EndFunction

Function setVisible(Int id, Int visible = 1)
	iWantWidgetsNative.SetVisible(id, visible)
EndFunction

Function setRotation(Int id, Int rotation)
	iWantWidgetsNative.SetRotation(id, rotation)
EndFunction

Function setTransparency(Int id, Int a)
	iWantWidgetsNative.SetTransparency(id, a)
EndFunction

Function setRGB(Int id, Int r, Int g, Int b)
	iWantWidgetsNative.SetRGB(id, r, g, b)
EndFunction

Function destroy(Int id)
	iWantWidgetsNative.Destroy(id)
EndFunction

Function drawShapeLine(Int[] list, Int XPos = 639, Int YPos = 359, Int XChange = 25, Int YChange = 25, Bool skipInvisible = True, Bool skipAlpha0 = True)
	iWantWidgetsNative.DrawShapeLine(list, XPos, YPos, XChange, YChange, skipInvisible, skipAlpha0)
EndFunction

Function drawShapeCircle(Int[] list, Int XPos = 639, Int YPos = 359, Int radius = 50, Int startAngle = 0, Int degreeChange = 45, Bool skipInvisible = True, Bool skipAlpha0 = True, Bool autoSpace = False)
	iWantWidgetsNative.DrawShapeCircle(list, XPos, YPos, radius, startAngle, degreeChange, skipInvisible, skipAlpha0, autoSpace)
EndFunction

Function drawShapeOrbit(Int[] list, Int XPos = 639, Int YPos = 359, Int radius = 50, Int startAngle = 0, Int degreeChange = 45, Bool skipInvisible = True, Bool skipAlpha0 = True, Bool autoSpace = False)
	iWantWidgetsNative.DrawShapeOrbit(list, XPos, YPos, radius, startAngle, degreeChange, skipInvisible, skipAlpha0, autoSpace)
EndFunction

Function doTransition(Int id, Int targetValue, Int frames = 60, String targetAttribute = "alpha", String easingClass = "none", String easingMethod = "none", Int delay = 0)
	; The original quietly ran doTransition at half speed (fps = 30 while
	; advertising 60-frame defaults). Preserved: consumers tuned durations
	; against that behavior.
	doTransitionByFrames(id, targetValue, frames, targetAttribute, easingClass, easingMethod, delay, fps = 30)
EndFunction

Function doTransitionByFrames(Int id, Int targetValue, Int frames = 120, String targetAttribute = "alpha", String easingClass = "none", String easingMethod = "none", Int delay = 0, Int fps = 60)
	Float seconds = (frames As Float) / (fps As Float)
	Float delaySeconds = (delay As Float) / (fps As Float)

	doTransitionByTime(id, targetValue, seconds, targetAttribute, easingClass, easingMethod, delaySeconds)
EndFunction

Function doTransitionByTime(Int id, Int targetValue, Float seconds = 2.0, String targetAttribute = "alpha", String easingClass = "none", String easingMethod = "none", Float delay = 0.0)
	Float target = targetValue As Float
	String attr

	If (targetAttribute == "x" || targetAttribute == "y" || targetAttribute == "xscale" || targetAttribute == "yscale" || targetAttribute == "rotation")
		attr = "_" + targetAttribute
	ElseIf targetAttribute == "meterpercent"
		target = targetValue / 100.0
		attr = "percent"
	Else
		; Default to alpha
		attr = "_alpha"
	EndIf

	String eClass
	String eMethod

	If (easingClass == "regular" || easingClass == "bounce" || easingClass == "back" || easingClass == "elastic" || easingClass == "strong")
		eClass = easingClass
	Else
		; Default to no easing
		eClass = "none"
	EndIf

	If (easingMethod == "in")
		eMethod = "easeIn"
	ElseIf easingMethod == "out"
		eMethod = "easeOut"
	ElseIf easingMethod == "inout"
		eMethod = "easeInOut"
	Else
		; If a valid easing method is not defined, revert to no easing
		eClass = "none"
		eMethod = ""
	EndIf

	iWantWidgetsNative.DoTransition(id, target, seconds, attr, eClass, eMethod, delay)
EndFunction

Function setAllVisible(Bool visible = True)
	iWantWidgetsNative.SetAllVisible(visible)
EndFunction

String Function _serializeArray(String[] a)
	; Legacy pipe protocol helper, kept for external callers.
	String s = ""
	Int i = 0
	While i < a.Length
		If i > 0
			s += "|"
		EndIf
		s += a[i]
		i += 1
	EndWhile
	Return s
EndFunction

Function logWidgetData(Int id)
	Debug.Trace("======logWidgetData Start=======")
	Debug.Trace("Widget ID: " + id)
	Debug.Trace("Width: " + iWantWidgetsNative.GetXSize(id))
	Debug.Trace("Height: " + iWantWidgetsNative.GetYSize(id))
	Debug.Trace("=======logWidgetData End========")
EndFunction

Function triggerReset()
	Debug.Trace("iWant Widgets (Prisma): ***LIBRARY RESET***")
	iWantWidgetsNative.Reset()
	RegisterForModEvent("iWantWidgetsReset", "OniWantWidgetsReset")
	SendModEvent("iWantWidgetsReset")
EndFunction

Event OniWantWidgetsReset(String eventName, String strArg, Float numArg, Form sender)
	Debug.Trace("iWant Widgets (Prisma): iWant Widgets Reset Event Fired")
EndEvent

; -----------------------------------------------------------------------------
; The setSkyrim* family manipulates the VANILLA Scaleform HUD directly via UI.*
; paths. It never went through the iWantWidgets Flash widget, so it is carried
; over verbatim and keeps working unchanged in the Prisma edition.
; -----------------------------------------------------------------------------

Function setSkyrimTemperature(Int level)
	;0 = Neutral
	;1 = Fire
	;2 = Warm
	;3 = Cold
	;4 = Freezing

	UI.InvokeInt("HUD Menu", "_root.HUDMovieBaseInstance.SetCompassTemperature", level)
	UI.Invoke   ("HUD Menu", "_root.HUDMovieBaseInstance.TemperatureMeterAnim")
EndFunction

Function setSkyrimHealthMeterPercent(Int percent)
	UI.InvokeInt("HUD Menu", "_root.HUDMovieBaseInstance.SetHealthMeterPercent", percent)
EndFunction

Function setSkyrimStaminaMeterPercent(Int percent)
	UI.InvokeInt("HUD Menu", "_root.HUDMovieBaseInstance.SetStaminaMeterPercent", percent)
EndFunction

Function setSkyrimMagickaMeterPercent(Int percent)
	UI.InvokeInt("HUD Menu", "_root.HUDMovieBaseInstance.SetMagickaMeterPercent", percent)
EndFunction

String Function _getSkyrimTargetBase(String element)
	String targetBase = ""

	If element == "health"
		targetBase = "_root.HUDMovieBaseInstance.Health."
	ElseIf element == "magicka"
		targetBase = "_root.HUDMovieBaseInstance.Magica."
	ElseIf element == "stamina"
		targetBase = "_root.HUDMovieBaseInstance.Stamina."
	ElseIf element == "enemyhealth"
		targetBase = "_root.HUDMovieBaseInstance.EnemyHealth."
	ElseIf element == "crosshair"
		targetBase = "_root.HUDMovieBaseInstance.CrosshairInstance."
	ElseIf element == "crosshairalert"
		targetBase = "_root.HUDMovieBaseInstance.CrosshairAlert."
	ElseIf element == "stealthmeter"
		targetBase = "_root.HUDMovieBaseInstance.StealthMeterInstance."
	ElseIf element == "questmarker"
		targetBase = "_root.HUDMovieBaseInstance.FloatingQuestMarker."
	ElseIf element == "compass"
		targetBase = "_root.HUDMovieBaseInstance.CompassShoutMeterHolder."
	EndIf

	Return(targetBase)
EndFunction

Function setSkyrimTransparency(String element, Int a = 100)
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_alpha"

	If targetBase != ""
		UI.SetInt(HUD_MENU, (targetBase + attribute), a)
	EndIf
EndFunction

Function setSkyrimZoom(String element, Int xscale = 100, Int yscale = 100)
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_xscale"

	If targetBase != ""
		UI.SetInt(HUD_MENU, (targetBase + attribute), xscale)
		attribute = "_yscale"
		UI.SetInt(HUD_MENU, (targetBase + attribute), yscale)
	EndIf
EndFunction

Function setSkyrimVisible(String element, Int visible = 1)
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_visible"

	If targetBase != ""
		UI.SetInt(HUD_MENU, (targetBase + attribute), visible)
	EndIf
EndFunction

Function _setSkyrimPos(String element, Int xpos = 0, Int ypos = 0)
	; This function is undocumented and included for experimentation only
	; Do not expect it to be available in all future releases
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_x"

	If targetBase != ""
		UI.SetInt(HUD_MENU, (targetBase + attribute), xpos)
		attribute = "_y"
		UI.SetInt(HUD_MENU, (targetBase + attribute), ypos)
	EndIf
EndFunction

Int Function _getSkyrimXPos(String element)
	; This function is undocumented and included for experimentation only
	; Do not expect it to be available in all future releases
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_x"

	If targetBase != ""
		Return(UI.GetInt(HUD_MENU, (targetBase + attribute)))
	EndIf
	Return 0
EndFunction

Int Function _getSkyrimYPos(String element)
	; This function is undocumented and included for experimentation only
	; Do not expect it to be available in all future releases
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_y"

	If targetBase != ""
		Return(UI.GetInt(HUD_MENU, (targetBase + attribute)))
	EndIf
	Return 0
EndFunction

Function _setSkyrimSize(String element, Int h, Int w)
	; This function is undocumented and included for experimentation only
	; Do not expect it to be available in all future releases
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_height"

	If targetBase != ""
		UI.SetInt(HUD_MENU, (targetBase + attribute), h)
		attribute = "_width"
		UI.SetInt(HUD_MENU, (targetBase + attribute), w)
	EndIf
EndFunction

Function _setSkyrimRotation(String element, Int rot = 0)
	; This function is undocumented and included for experimentation only
	; Do not expect it to be available in all future releases
	String targetBase = _getSkyrimTargetBase(element)
	String attribute = "_rotation"

	If targetBase != ""
		UI.SetInt(HUD_MENU, (targetBase + attribute), rot)
	EndIf
EndFunction

Event OnWidgetReset()
	; Kept for signature compatibility with the SkyUI-based original. Nothing
	; fires this in the Prisma edition; the player alias drives triggerReset().
	triggerReset()
EndEvent

String Function GetWidgetSource()
	Return("PrismaUI/views/iwantwidgets/index.html")
EndFunction

String Function GetWidgetType()
	; Must be the same as script name
	Return "iWant_Widgets"
EndFunction
