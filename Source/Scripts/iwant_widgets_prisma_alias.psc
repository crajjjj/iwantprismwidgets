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

	; Each triggerReset WIPES the view and every consumer reloads all its icons
	; -- so fire as FEW times as possible. Firing again before a reload has
	; finished wipes it mid-flight, leaving icons un-sized (big), un-tinted
	; (white) or un-painted (empty), and the churn reads as flicker. So: fire
	; once after consumers have had time to register, then wait LONG enough for
	; the reload to complete and ack before even considering a single retry.
	; Two fires max -- the retry is only the safety net for a consumer whose
	; quest inits unusually late (the init-race this whole dance exists for).
	Float[] gaps = new Float[2]
	gaps[0] = 2.5    ; initial: let consumers register their reset handler
	gaps[1] = 8.0    ; lone retry, only if nobody has acked by then

	Int i = 0
	While i < gaps.Length
		Utility.Wait(gaps[i])
		If _resetGen != myGen
			Return
		EndIf
		; If a consumer already finished loading (its own reload, or a prior
		; fire), no reset is needed at all -- don't wipe a good state.
		If _consumerReady
			Return
		EndIf

		iWant_Widgets w = GetOwningQuest() As iWant_Widgets
		If w
			w.triggerReset()
		EndIf

		; Wait long enough for the consumer's full reload (many loadWidget calls
		; with internal waits) to finish and send its ack, so the next retry
		; never overlaps an in-flight reload.
		Utility.Wait(5.0)
		If _resetGen != myGen || _consumerReady
			Return
		EndIf

		i += 1
	EndWhile
EndFunction
