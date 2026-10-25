-- Uninstall ODDKIN — removes the VST3 + AU and (optionally) your saved patches.
set sysVst3 to "/Library/Audio/Plug-Ins/VST3/ODDKIN.vst3"
set sysAu to "/Library/Audio/Plug-Ins/Components/ODDKIN.component"
set homePath to POSIX path of (path to home folder)
set userVst3 to homePath & "Library/Audio/Plug-Ins/VST3/ODDKIN.vst3"
set userAu to homePath & "Library/Audio/Plug-Ins/Components/ODDKIN.component"
set appFolder to "/Applications/Oddkin"
-- leftovers from when it was called JACKPOT
set oldStuff to "/Library/Audio/Plug-Ins/VST3/JACKPOT.vst3 /Library/Audio/Plug-Ins/Components/JACKPOT.component '/Applications/Jackpot Audio'"
set patchFolder to (POSIX path of (path to documents folder)) & "ODDKIN"

set msg to "This removes the ODDKIN plug-in (VST3 and Audio Unit) from this Mac." & return & return & ¬
	"Your saved patches are in Documents/ODDKIN. Do you want to keep them?" & return & return & ¬
	"Quit Ableton first."
set choice to button returned of (display dialog msg buttons {"Cancel", "Remove everything", "Keep my patches"} ¬
	default button "Keep my patches" cancel button "Cancel" with title "Uninstall ODDKIN" with icon caution)

-- system-wide copies (from the installer) need an admin password
set needsAdmin to ((do shell script "if [ -e " & quoted form of sysVst3 & " ] || [ -e " & quoted form of sysAu & " ] || [ -e " & quoted form of appFolder & " ] || [ -e /Library/Audio/Plug-Ins/VST3/JACKPOT.vst3 ] || [ -e /Library/Audio/Plug-Ins/Components/JACKPOT.component ]; then echo yes; else echo no; fi") is "yes")

if needsAdmin then
	try
		do shell script "rm -rf " & quoted form of sysVst3 & " " & quoted form of sysAu & " " & quoted form of appFolder & " " & oldStuff & ¬
			"; for id in vst3 au uninstaller; do pkgutil --forget com.oddkin.oddkin.$id >/dev/null 2>&1; done; true" with administrator privileges
	on error errMsg number errNum
		if errNum is -128 then return -- cancelled the password prompt
		display dialog "Couldn't remove ODDKIN:" & return & errMsg buttons {"OK"} default button "OK" with title "Uninstall ODDKIN" with icon stop
		return
	end try
end if

-- per-user copies (from build_mac.sh)
do shell script "rm -rf " & quoted form of userVst3 & " " & quoted form of userAu & "; killall -9 AudioComponentRegistrar >/dev/null 2>&1; true"

if choice is "Remove everything" then
	do shell script "rm -rf " & quoted form of patchFolder
	set extra to "Your saved patches were deleted too."
else
	set extra to "Your saved patches are still in Documents/ODDKIN."
end if

display dialog "ODDKIN has been uninstalled." & return & extra & return & return & ¬
	"Restart Ableton (or Rescan plug-ins) to finish." buttons {"OK"} default button "OK" with title "Uninstall ODDKIN"
