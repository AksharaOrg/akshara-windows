$ErrorActionPreference = 'Stop'
if (-not (Get-Command magick -ErrorAction SilentlyContinue)) { throw 'ImageMagick magick is required' }
magick assets/source/AksharaIconMaster.png -crop 820x820+102+102 +repage -colorspace Gray -threshold 35% -negate -resize 38x38 -background white -gravity center -extent 44x44 -bordercolor '#808080' -border 1 assets/ime/AksharaInputMaster.png
magick assets/ime/AksharaInputMaster.png -define icon:auto-resize=48,40,32,24,20,16 assets/ime/AksharaInput.ico
# The app icon and tiles need their white corners cut out: tools/generate-app-icons.py.
python "$PSScriptRoot/generate-app-icons.py"
if ($LASTEXITCODE) { throw 'generate-app-icons.py failed' }

