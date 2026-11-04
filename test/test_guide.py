"""Every illustrated guide step must point to an embedded, existing asset."""
from html.parser import HTMLParser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ImageReferences(HTMLParser):
    def __init__(self):
        super().__init__()
        self.images = []

    def handle_starttag(self, tag, attrs):
        if tag == 'img':
            self.images.append(dict(attrs).get('src'))

class GuideAssets(unittest.TestCase):
    def test_illustrations_exist_and_are_real_captures(self):
        html = (ROOT / 'data/guide-fa.html').read_text(encoding='utf-8')
        parser = ImageReferences()
        parser.feed(html)
        self.assertGreaterEqual(len(parser.images), 10)
        for ref in parser.images:
            self.assertTrue(ref.startswith('/guide/'), ref)
            path = ROOT / 'data' / ref.lstrip('/')
            self.assertTrue(path.is_file() and path.stat().st_size > 1000, ref)
            if ref.endswith('.jpg'):
                self.assertEqual(path.read_bytes()[:3], b'\xff\xd8\xff', ref)

    def test_vazirmatn_is_bundled(self):
        css = (ROOT / 'data/guide-fa.css').read_text(encoding='utf-8')
        self.assertIn('font-family:Vazirmatn', css)
        for name in ('arabic-400','arabic-700','latin-400','latin-700'):
            self.assertTrue((ROOT / f'data/fonts/vazirmatn-{name}.woff2').is_file())
        self.assertTrue((ROOT/'data/fonts/OFL.txt').is_file())

if __name__ == '__main__':
    unittest.main()
