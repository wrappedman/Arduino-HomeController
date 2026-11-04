"""Static release sanity checks; not a substitute for firmware/hardware tests."""
from pathlib import Path
import csv
import re
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

    def test_arduino_custom_partition_and_board_menu_documented(self):
        guide=(ROOT/'docs/install-fa.md').read_text()
        self.assertIn('ESP32S3 Dev Module',guide)
        self.assertIn('16MB',guide)
        self.assertIn('OPI',guide)
        self.assertIn('QIO',guide)
        self.assertIn('16M Flash (3MB APP/9.9MB FATFS)',guide)
        workflow=(ROOT/'.github/workflows/ci.yml').read_text()
        self.assertIn('arduino-cli compile',workflow)
        self.assertIn('PartitionScheme=app3M_fat9M_16MB',workflow)
        self.assertNotIn('pio run',workflow)

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

    def test_arduino_sketch_contains_all_firmware_and_assets(self):
        sketch=ROOT/'arduino/SHAP'
        self.assertTrue((sketch/'SHAP.ino').exists())
        self.assertEqual((sketch/'partitions.csv').read_bytes(), (ROOT/'partitions/ota_16mb.csv').read_bytes())
        for file in (ROOT/'src').iterdir():
            if file.suffix in ('.cpp','.h'):
                self.assertEqual(file.read_bytes(),(sketch/file.name).read_bytes(),file.name)
        import hashlib
        embedded=(ROOT/'src/EmbeddedAssets.cpp').read_text()
        for data in (ROOT/'data').rglob('*'):
            if data.is_file():
                digest=hashlib.sha256(data.read_bytes()).hexdigest()
                self.assertIn(f'// /{data.relative_to(ROOT/"data").as_posix()} sha256={digest}',embedded)
        api=(ROOT/'src/Api.cpp').read_text()
        self.assertIn('findEmbeddedAsset(path)',api)
        self.assertNotIn('LittleFS.open(path',api)

    def test_config_pool_is_compacted_before_every_mutation(self):
        """ArduinoJson 6 never returns slots when an object is replaced, so the
        fixed 32 KB config document would fill up after a handful of edits and every
        later saveConfig() would fail. Verified by simulation: 14 device edits at
        maximum configuration, a single dashboard save with 48 widgets."""
        storage = (ROOT/'src/Storage.cpp').read_text()
        api = (ROOT/'src/Api.cpp').read_text()
        self.assertIn('void compactConfig()', storage)
        self.assertIn('config.garbageCollect()', storage)
        # every edit path must reclaim first, and must fall back to a safe rollback
        self.assertIn('compactConfig();', api)
        self.assertIn('restoreConfig(', api)
        self.assertNotIn('config.set(previous);', api)
        self.assertNotIn('config.set(old);', api)
        # saveConfig retries after compacting instead of failing permanently
        save = storage[storage.index('bool saveConfig()'):]
        self.assertIn('garbageCollect()', save)

    def test_device_edits_do_not_reset_unrelated_outputs(self):
        """Editing one device used to re-initialise every GPIO and zero every value,
        which blanked virtual device readings and briefly floated every relay pin."""
        api = (ROOT/'src/Api.cpp').read_text()
        hardware = (ROOT/'src/Hardware.cpp').read_text()
        self.assertIn('initDevice(existing)', api)
        self.assertIn('void initDevice(', hardware)
        self.assertIn('void releaseDevice(', hardware)
        self.assertIn('void syncHardware(', hardware)
        # initHardware() is the boot-time safe-state entry point only
        edited = api[api.index('if (web.method()==HTTP_POST || web.method()==HTTP_PUT)'):]
        self.assertNotIn('initHardware()', edited)

    def test_reserved_pins_covered_in_both_layers(self):
        firmware=(ROOT/'src/Hardware.cpp').read_text()
        frontend=(ROOT/'data/app.js').read_text()
        for gpio in (8,9,10,11,12,13):
            self.assertIn(f'pin != {gpio}',firmware)
            self.assertRegex(frontend,rf'\b{gpio}\b')
        self.assertIn('return pin >= 1 && pin <= 10',firmware)

if __name__=='__main__':
    unittest.main()
