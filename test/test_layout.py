"""Static release sanity checks; not a substitute for firmware/hardware tests."""
from pathlib import Path
import csv
import re
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ReleaseLayout(unittest.TestCase):
    def test_ota_partitions_do_not_overlap_or_exceed_flash(self):
        partitions=[]
        with (ROOT/'partitions/ota_16mb.csv').open() as f:
            for row in csv.reader(line for line in f if not line.lstrip().startswith('#')):
                _, kind, subtype, offset, size, *_=map(str.strip,row)
                start=int(offset,0)
                end=start+int(size,0)
                self.assertEqual(start % 0x1000,0)
                self.assertLessEqual(end,16*1024*1024)
                partitions.append((start,end,kind,subtype))
        for a,b in zip(sorted(partitions),sorted(partitions)[1:]):
            self.assertLessEqual(a[1],b[0])
        self.assertEqual(sum(k=='app' for _,_,k,_ in partitions),2)
        self.assertEqual([b-a for a,b,k,_ in partitions if k=='app'],[0x500000]*2)

    def test_board_manifest_matches_flash_and_app_partition(self):
        board=json.loads((ROOT/'boards/shap-esp32-s3-n16r8.json').read_text())
        ini=(ROOT/'platformio.ini').read_text()
        self.assertIn('board = shap-esp32-s3-n16r8',ini)
        self.assertEqual(board['upload']['flash_size'],'16MB')
        self.assertEqual(board['upload']['maximum_size'],0x500000)
        self.assertEqual(board['build']['arduino']['memory_type'],'qio_opi')

    def test_embedded_assets_are_local_and_bounded(self):
        index=(ROOT/'data/index.html').read_text()
        self.assertNotRegex(index,r'<(?:script|link)[^>]+(?:https?:)?//')
        self.assertLess(sum(p.stat().st_size for p in (ROOT/'data').rglob('*') if p.is_file()),3*1024*1024)
        for filename in ('index.html','app.css','app.js'):
            self.assertLess((ROOT/'data'/filename).stat().st_size,128*1024)

    def test_recovery_button_is_nonblocking_and_after_boot(self):
        main=(ROOT/'src/main.cpp').read_text()
        network=(ROOT/'src/Network.cpp').read_text()
        self.assertIn('shap::tickRecoveryButton();',main)
        self.assertNotIn('while (digitalRead(0)',main)
        self.assertIn('millis()-pressedAt>=5000',network)
        self.assertIn('millis()-bootMs<2000',network)

    def test_reserved_pins_covered_in_both_layers(self):
        firmware=(ROOT/'src/Hardware.cpp').read_text()
        frontend=(ROOT/'data/app.js').read_text()
        for gpio in (8,9,10,11,12,13):
            self.assertIn(f'pin != {gpio}',firmware)
            self.assertRegex(frontend,rf'\b{gpio}\b')
        self.assertIn('return pin >= 1 && pin <= 10',firmware)

if __name__=='__main__':
    unittest.main()
