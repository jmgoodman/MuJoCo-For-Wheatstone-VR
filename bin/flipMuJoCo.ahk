#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
; #Warn  ; Enable warnings to assist with detecting common errors.
SendMode Input  ; Recommended for new scripts due to its superior speed and reliability.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.
; https://autohotkey.com/board/topic/71132-rotate-only-one-entire-window/

#SingleInstance Force

; Uncomment if Gdip.ahk is not in your standard library
#Include, Gdip_All.ahk

pToken := Gdip_Startup()

; open a paint window
Run, "C:\Program Files (x86)\Northern Digital Inc\WaveFront\WaveFront.exe","C:\Program Files (x86)\Northern Digital Inc\WaveFront\",Hide
sleep, 10000

SetWorkingDir, "C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin\" ; needed to put this script directly in the bin folder to avoid licensing issues.
sleep, 1000
Run, "C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin\basic_withTCPclient.exe" "..\model\scene_custom.xml","C:\Users\neuro-setup\Documents\GitHub\MuJoCo-For-Wheatstone-VR\bin"
sleep, 3000

Gui, 1: +LastFound
hwnd := WinExist("ahk_exe basic_withTCPclient.exe")
; Gui, 1: Add, Edit, x10 y10 w900 h600, Here is some text
WinMove,,,0,0,3840,1200
Gui, 1: Show, AutoSize, Window 1

WinGetPos,,, w, h, ahk_id %hwnd%

Gui, 2: -Caption +E0x80000 +LastFound +OwnDialogs +Owner +AlwaysOnTop
Gui, 2: Show, NA
hwnd2 := WinExist()

hbm := CreateDIBSection(w, h), hdc := CreateCompatibleDC(), obm := SelectObject(hdc, hbm), G := Gdip_GraphicsFromHDC(hdc)

UpdateLayeredWindow(hwnd2, hdc, 0, 0, w, h)
SetTimer, Update, 16 ; doesn't seem to run very quickly after all...
return

;#####################################################################################

Update:

; grab window, not screen
pBitmap := Gdip_BitmapFromScreen("hwnd:" hwnd) ; todo: find a way to capture two windows and flip each individually, rather than flipping one big window (to avoid needing to swap L and R eyes in the mujoco xml model file)

; grab bitmap from screen, not window
; pBitmap := Gdip_BitmapFromScreen("0|0|700|400")

; RotateNoneFlipNone   = 0
; Rotate90FlipNone     = 1
; Rotate180FlipNone    = 2
; Rotate270FlipNone    = 3
; RotateNoneFlipX      = 4
; Rotate90FlipX        = 5
; Rotate180FlipX       = 6
; Rotate270FlipX       = 7
; RotateNoneFlipY      = Rotate180FlipX
; Rotate90FlipY        = Rotate270FlipX
; Rotate180FlipY       = RotateNoneFlipX
; Rotate270FlipY       = Rotate90FlipX
; RotateNoneFlipXY     = Rotate180FlipNone
; Rotate90FlipXY       = Rotate270FlipNone
; Rotate180FlipXY      = RotateNoneFlipNone
; Rotate270FlipXY      = Rotate90FlipNone

Gdip_ImageRotateFlip(pBitmap, 4)

Gdip_DrawImage(G, pBitmap)
UpdateLayeredWindow(hwnd2, hdc)
Gdip_DisposeImage(pBitmap)
return

;#####################################################################################

GuiClose:
SelectObject(hdc, obm), DeleteObject(hbm), DeleteDC(hdc)
Gdip_DeleteGraphics(G)
Gdip_Shutdown(pToken)
ExitApp
return