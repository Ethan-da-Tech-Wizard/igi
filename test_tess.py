# Program: igi - test_tess.py
# Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
# Last Modified: 2026-11-18
# Purpose: Manual script that draws text on an image and prints the text pytesseract reads from it.

from PIL import Image, ImageDraw, ImageFont
import pytesseract
import sys

img = Image.new('RGB', (400, 100), color = (30, 30, 30))
d = ImageDraw.Draw(img)
# just draw some text
d.text((20,50), '1. The "Crash" on the first character', fill=(255,255,255))
img.save('/tmp/test.png')
print("OCR output:", pytesseract.image_to_string(img))
