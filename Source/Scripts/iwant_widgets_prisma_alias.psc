Scriptname iwant_widgets_prisma_alias extends ReferenceAlias
{Player alias that replaces SkyUI's OnWidgetReset as the reset trigger: on
 every game load it invalidates old widget ids and re-broadcasts the
 iWantWidgetsReset mod event that consumers rebuild from.}

Float _lastResetTime = -100.0

Event OnInit()
	_fireReset()
EndEvent

Event OnPlayerLoadGame()
	_fireReset()
EndEvent

Function _fireReset()
	; OnInit and OnPlayerLoadGame can both fire on the load that first
	; installs the quest; debounce so consumers only rebuild icons once.
	; The stamp is taken BEFORE the wait: Utility.Wait unlocks this script
	; instance, so a second event thread would otherwise slip in mid-wait.
	Float now = Utility.GetCurrentRealTime()
	If now - _lastResetTime < 3.0
		Return
	EndIf
	_lastResetTime = now

	; Consumers like iWant Status Bars re-register their iWantWidgetsReset
	; handler from their own load hooks. Alias event order across mods is
	; undefined, so give them a beat before broadcasting -- the SkyUI path
	; this replaces was similarly delayed behind the HUD menu reload.
	Utility.Wait(1.5)
	iWant_Widgets w = GetOwningQuest() As iWant_Widgets
	If w
		w.triggerReset()
	EndIf
EndFunction
