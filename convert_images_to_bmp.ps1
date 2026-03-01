# Convert PNG graphics to BMP format for game loading
Add-Type -AssemblyName System.Drawing                  # Load image handling

$graphics_folder = "src/GRAPHICS_SOUNDS/GRAPHICS"      # Graphics folder path
$png_files = @("BOSS_ENTITY.png","NORMAL_ENTITIY.png","RED_AGRESSIVE_ENTITIY.png")  # Files to convert

Write-Host "Converting PNG to BMP format..." -ForegroundColor Green

foreach ($png_file in $png_files) {
    $png_path = Join-Path $graphics_folder $png_file   # Full PNG path
    $bmp_file = [System.IO.Path]::GetFileNameWithoutExtension($png_file) + ".bmp"  # Output filename
    $bmp_path = Join-Path $graphics_folder $bmp_file   # Full BMP path
    
    if (Test-Path $png_path) {
        Write-Host "Converting: $png_file -> $bmp_file" -ForegroundColor Cyan
        $image = [System.Drawing.Image]::FromFile($png_path)  # Load PNG
        $image.Save($bmp_path, [System.Drawing.Imaging.ImageFormat]::Bmp)  # Save as BMP
        $image.Dispose()                                # Free memory
        Write-Host "  Created: $bmp_file" -ForegroundColor Green
    } else {
        Write-Host "  Not found: $png_path" -ForegroundColor Yellow
    }
}

Write-Host "Done!" -ForegroundColor Green

