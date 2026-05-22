"""
Scintilla preset scripting runtime.

Built-in and user presets import the Preset base class from here:

    from led_cube import Preset

The on-the-wire details (subprocess JSON protocol, audio namespace) are
handled by led_cube._runner — preset authors do not need to know about
them.
"""

from __future__ import annotations

from ._cube_proxy import CubeProxy


class Preset:
    """Base class for Scintilla audio-reactive presets.

    Subclass and override on_load(), optionally on_beat(), and on_frame().
    Class attributes name / description / author / tags are surfaced in
    the preset browser.

    Example:

        class MyPreset(Preset):
            name        = "My Preset"
            description = "What it does"
            author      = "You"
            tags        = ["beat", "ambient"]

            def on_load(self, cube):
                self.state = 0.0

            def on_beat(self, cube, audio):
                self.state = audio.bass

            def on_frame(self, cube, audio):
                cube.set_hsv(audio.centroid * 360, 1.0, self.state)
                self.state *= 0.9
    """

    # ── Metadata surfaced in the preset browser ─────────────────────────────
    name:        str             = "Untitled"
    description: str             = ""
    author:      str             = ""
    tags:        list[str]       = []

    # ── Lifecycle callbacks (override in subclass) ──────────────────────────

    def on_load(self, cube) -> None:
        """Called once after the cube proxy is ready. Initialise state here."""
        pass

    def on_beat(self, cube, audio) -> None:
        """Called once per detected audio onset. Optional — default no-op."""
        pass

    def on_frame(self, cube, audio) -> None:
        """Called once per render frame. Update cube colours here."""
        pass


__all__ = ["Preset", "CubeProxy"]
