"""Unit tests for the helloworld relay's line routing (run: /usr/bin/python3 -m unittest)."""
import json
import threading
import unittest

from server import Link


class FakeSink:
    def __init__(self):
        self.events = []

    def __call__(self, event):
        self.events.append(event)


class LinkRouting(unittest.TestCase):
    def setUp(self):
        self.sent = []
        self.sink = FakeSink()
        self.link = Link(send=self.sent.append, broadcast=self.sink)

    def test_push_lines_are_broadcast(self):
        self.link.feed_line('{"f":1,"num":0,"face":[],"running":"Face Detector"}')
        self.link.feed_line('{"hb":{"t":70}}')
        self.assertEqual([e["f"] if "f" in e else "hb" for e in self.sink.events], [1, "hb"])

    def test_reply_goes_to_waiting_request_not_broadcast(self):
        result = {}

        def ask():
            result["r"] = self.link.request({"do": "ping"}, timeout=2)

        t = threading.Thread(target=ask)
        t.start()
        for _ in range(100):
            if self.sent:
                break
            threading.Event().wait(0.01)
        req = json.loads(self.sent[0])
        self.assertEqual(req["do"], "ping")
        self.link.feed_line(json.dumps({"id": req["id"], "ok": True}))
        t.join()
        self.assertEqual(result["r"], {"id": req["id"], "ok": True})
        self.assertEqual(self.sink.events, [])

    def test_request_ids_are_unique(self):
        self.link.request({"do": "ping"}, timeout=0.01)
        self.link.request({"do": "ping"}, timeout=0.01)
        ids = [json.loads(s)["id"] for s in self.sent]
        self.assertEqual(len(set(ids)), 2)

    def test_request_times_out(self):
        self.assertIsNone(self.link.request({"do": "ping"}, timeout=0.05))

    def test_garbage_and_unmatched_replies_are_ignored(self):
        self.link.feed_line("not json")
        self.link.feed_line('{"id":999,"ok":true}')
        self.assertEqual(self.sink.events, [])

    def test_request_rejects_caller_supplied_id(self):
        self.link.request({"id": 5, "do": "ping"}, timeout=0.01)
        self.assertNotEqual(json.loads(self.sent[0])["id"], 5)


if __name__ == "__main__":
    unittest.main()


class ReplayTest(unittest.TestCase):
    def test_replays_lines_in_order_and_skips_blanks(self):
        import os
        import tempfile
        from server import Replay
        fd, path = tempfile.mkstemp()
        with os.fdopen(fd, "w") as f:
            f.write('{"f":1}\n\n{"hb":{"t":70}}\n{"f":2}\n')
        got = []
        Replay(path, period=0).run_once(got.append)
        os.unlink(path)
        self.assertEqual(got, ['{"f":1}', '{"hb":{"t":70}}', '{"f":2}'])
