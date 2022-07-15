#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
; #Warn  ; Enable warnings to assist with detecting common errors.
SendMode Input  ; Recommended for new scripts due to its superior speed and reliability.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.
; https://autohotkey.com/board/topic/71132-rotate-only-one-entire-window/
; btw it'd be better to just do this as a windows batch script...

#SingleInstance Force

; open the proprietary software that reads from the magnetic tracking system
; commented out for those without access to it
; Run, "C:\Program Files (x86)\Northern Digital Inc\WaveFront\WaveFront.exe","C:\Program Files (x86)\Northern Digital Inc\WaveFront\",Hide
; sleep, 20000

SetWorkingDir, %A_ScriptDir% ; needed to put this script directly in the bin folder to avoid licensing issues.
sleep, 1000
Run, %A_ScriptDir%\basic_withTCPclient.exe "..\model\scene_custom.xml","C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin"

return