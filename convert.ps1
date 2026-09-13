Add-Type -AssemblyName System.Drawing
$p = 'C:\Users\mat\.gemini\antigravity-ide\brain\68b0249d-68e0-4d17-96d8-e8536e3710d0\ktna_menu_screenshot'
if (Test-Path "$p.bmp") {
    $b = [System.Drawing.Image]::FromFile("$p.bmp")
    $b.Save("$p.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $b.Dispose()
    Write-Host "OK"
}
