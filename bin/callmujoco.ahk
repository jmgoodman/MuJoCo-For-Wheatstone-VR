#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
; #Warn  ; Enable warnings to assist with detecting common errors.
SendMode Input  ; Recommended for new scripts due to its superior speed and reliability.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.
; https://autohotkey.com/board/topic/71132-rotate-only-one-entire-window/

#SingleInstance Force

; open a paint window
Run, "C:\Program Files (x86)\Northern Digital Inc\WaveFront\WaveFront.exe","C:\Program Files (x86)\Northern Digital Inc\WaveFront\",Hide
sleep, 20000

SetWorkingDir, "C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin\" ; needed to put this script directly in the bin folder to avoid licensing issues.
sleep, 1000
Run, "C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin\basic_withTCPclient.exe" "..\model\scene_custom.xml","C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin"

return