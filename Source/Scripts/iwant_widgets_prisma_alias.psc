Scriptname iwant_widgets_prisma_alias extends ReferenceAlias
{Player alias that replaces SkyUI's OnWidgetReset as the reset trigger: on
 every game load it invalidates old widget ids and re-broadcasts the
 iWantWidgetsReset mod event that consumers rebuild from.}

Event OnInit()
	_fireReset()
EndEvent

Event OnPlayerLoadGame()
	_fireReset()
EndEvent

Function _fireReset()
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
