Scriptname iwant_widgets_prisma_alias extends ReferenceAlias
{Player alias that replaces SkyUI's OnWidgetReset as the reset trigger: on
 every game load it invalidates old widget ids and (re)broadcasts the
 iWantWidgetsReset mod event that consumers rebuild from.

 The broadcast is a bounded RETRY BURST, not a single shot. A consumer
 (iWant Status Bars, DD, other addons) registers for iWantWidgetsReset in its
 own OnInit/load hook, and mod-init order across mods is undefined -- if the
 provider fired only once and a consumer registered a moment later, it would
 miss the event forever and nothing would load. So we re-fire on a schedule,
 stopping as soon as a consumer signals it finished loading.}

Int _resetGen = 0
Bool _consumerReady = False

Event OnInit()
	_register()
	_startReset()
EndEvent

Event OnPlayerLoadGame()
	_register()
	_startReset()
EndEvent

Function _register()
	; Status Bars sends this when it finishes (re)loading its icons. Any
	; consumer that emits it lets us stop the retry burst early; consumers
	; that don't just ride out the full (bounded) schedule.
	RegisterForModEvent("iWantStatusBarsReady", "OnConsumerReady")
EndFunction

Event OnConsumerReady(String eventName, String strArg, Float numArg, Form sender)
	_consumerReady = True
EndEvent

Function _startReset()
	; A newer load (OnInit + OnPlayerLoadGame both firing, or a reload during
	; the burst) supersedes an in-flight burst via this generation token --
	; the old While loop sees the mismatch after its next Wait and bails.
	_resetGen += 1
	Int myGen = _resetGen
	_consumerReady = False

	; Inter-fire gaps (seconds). Front-loaded: most races resolve in the first
	; few seconds; the long tail covers a consumer whose quest inits very late.
	Float[] gaps = new Float[7]
	gaps[0] = 0.5
	gaps[1] = 1.5
	gaps[2] = 2.0
	gaps[3] = 3.0
	gaps[4] = 4.0
	gaps[5] = 6.0
	gaps[6] = 8.0

	Int i = 0
	While i < gaps.Length
		Utility.Wait(gaps[i])
		If _resetGen != myGen
			Return
		EndIf

		iWant_Widgets w = GetOwningQuest() As iWant_Widgets
		If w
			w.triggerReset()
		EndIf

		; Give the consumer a beat to catch the event, reload, and ack before
		; deciding whether another retry is needed -- avoids re-wiping the view
		; (a visible flicker) once someone has successfully loaded.
		Utility.Wait(1.0)
		If _resetGen != myGen || _consumerReady
			Return
		EndIf

		i += 1
	EndWhile
EndFunction
