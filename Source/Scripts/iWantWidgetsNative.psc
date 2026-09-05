Scriptname iWantWidgetsNative Hidden
{Native bridge into iWantWidgetsPrisma.dll, which renders through a PrismaUI
 view. Consumers should keep calling iWant_Widgets; this script is the
 internal transport and its surface may change between releases.}

; True once the PrismaUI view exists and its DOM is ready. Calls made earlier
; are queued DLL-side, so this is a convenience gate, not a hard requirement.
Bool Function IsReady() Global Native

; Loaders return the new widget id (ids are allocated natively, first id is 1).
; File paths are relative to Data/Interface/exported/, matching the original
; Flash root. DDS (including BC-compressed) is decoded in the DLL; PNG/JPG
; pass through. Loose files and BSA contents both resolve.
Int Function LoadWidget(String filename, Int xpos, Int ypos, Bool visible) Global Native
Int Function LoadText(String displayString, String font, Int size, Int xpos, Int ypos, Bool visible) Global Native
Int Function LoadMeter(Int xpos, Int ypos, Bool visible) Global Native

Function SetText(Int id, String displayString) Global Native
Function AppendText(Int id, String displayString) Global Native

Function SetPos(Int id, Int xpos, Int ypos) Global Native
Function SetSize(Int id, Int h, Int w) Global Native
Int Function GetXSize(Int id) Global Native
Int Function GetYSize(Int id) Global Native
Function SetZoom(Int id, Int xscale, Int yscale) Global Native
Function SetVisible(Int id, Int visible) Global Native
Function SetRotation(Int id, Int rotation) Global Native
Function SetTransparency(Int id, Int a) Global Native
Function SetRGB(Int id, Int r, Int g, Int b) Global Native

Function SendToBack(Int id) Global Native
Function SendToFront(Int id) Global Native
Function SwapDepths(Int id1, Int id2) Global Native

Function Destroy(Int id) Global Native
Function SetAllVisible(Bool visible) Global Native

Function DrawShapeLine(Int[] list, Int xpos, Int ypos, Int xchange, Int ychange, Bool skipInvisible, Bool skipAlpha0) Global Native
Function DrawShapeCircle(Int[] list, Int xpos, Int ypos, Int radius, Int startAngle, Int degreeChange, Bool skipInvisible, Bool skipAlpha0, Bool autoSpace) Global Native
Function DrawShapeOrbit(Int[] list, Int xpos, Int ypos, Int radius, Int startAngle, Int degreeChange, Bool skipInvisible, Bool skipAlpha0, Bool autoSpace) Global Native

; attr is pre-mapped by iWant_Widgets: _x, _y, _xscale, _yscale, _rotation,
; _alpha, or percent (meters, target already normalized to 0-1).
; easingClass: none|regular|bounce|back|elastic|strong; easingMethod:
; easeIn|easeOut|easeInOut or empty.
Function DoTransition(Int id, Float targetValue, Float seconds, String attr, String easingClass, String easingMethod, Float delay) Global Native

Function SetMeterPercent(Int id, Int percent) Global Native
Function SetMeterFillDirection(Int id, String direction) Global Native
Function DoMeterFlash(Int id) Global Native
Function SetMeterColors(Int id, Int lightRGB, Int darkRGB, Int flashRGB) Global Native

; Invalidates every existing widget id and clears the view. Fired on game
; load before the iWantWidgetsReset mod event goes out.
Function Reset() Global Native

; True until the first Reset of this game launch. The view persists across
; save-loads within a launch, so a reset (which makes every consumer reload
; all widgets) is only needed once per launch. The reset trigger gates on this.
Bool Function NeedsResync() Global Native
