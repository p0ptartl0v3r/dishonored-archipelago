# Manual snapshot

The original Manual apworld (`manual_dishonored_greenranger1200`), its YAML
template and the player documentation, kept for reference and as the fallback
way to play.

The Manual's `items.json`, `locations.json` and `regions.json` were copied to
`worlds/dishonored/data/`, which is now the source of truth. If you change logic
there and still want to ship the Manual, copy the files back into
`manual_dishonored_greenranger1200/data/` and zip that folder as
`manual_dishonored_greenranger1200.apworld`.

This folder is greenranger1200's work (their Manual apworld for Dishonored, its YAML template
and documentation), not covered by this repository's MIT license. The Manual framework code
inside the apworld keeps its own license.
