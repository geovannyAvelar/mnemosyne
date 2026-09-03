# Writing a Mnemosyne plugin

Mnemosyne can load small JavaScript plugins (via [QuickJS](https://github.com/quickjs-ng/quickjs))
that add an export format or react to reading/highlighting activity. This
document is the whole API — it's deliberately small.

## Installing a plugin

A plugin is a folder containing a `manifest.json` and a JS entry file:

```json
{
  "id": "example.word-count",
  "name": "Word Count Exporter",
  "version": "1.0.0",
  "description": "Exports a plain word-count summary.",
  "main": "main.js"
}
```

Drop the folder into Mnemosyne's Plugins folder (**Plugins → Manage
Plugins... → Open Plugins Folder**), then enable it in that same dialog.
`id` must be unique among your installed plugins.

## What a plugin can do

That's it — no file, network, or process access is exposed. A plugin can
only read what's passed into a hook and call the three functions below.

### `mnemosyne.registerExporter(options)`

Adds an entry to **File → Export Notes** (and the library-wide export
variant). Call this at the top level of your entry file.

```js
mnemosyne.registerExporter({
  id: "word-count",              // combined with your plugin id: "example.word-count.word-count"
  label: "as Word Count...",     // menu item text
  fileFilter: "Text Files (*.txt)",
  defaultExtension: "txt",
  format: function (bookTitle, entries) {
    // entries: [{ text, note, color, targetIndex, positionLabel, createdAt }, ...]
    const words = entries.reduce((n, e) => n + e.text.split(/\s+/).length, 0);
    return bookTitle + ": " + words + " highlighted words";
  }
});
```

`entries` is already sorted in reading order. `positionLabel` is a
human-readable string like `"Page 12"`/`"Chapter 3"`/`"Part 3"`, or `""` for
formats with no discrete page/chapter unit (Markdown, plain text). `format`
must return a string — that string is written verbatim to the file the user
picks in the save dialog.

### `mnemosyne.on(eventName, callback)`

Reacts to something happening in the app. `callback` receives one payload
object; return values are ignored.

| Event               | Payload                                                  |
| -------------------- | --------------------------------------------------------- |
| `"documentOpened"`   | `{ bookHash, title, format }`                              |
| `"highlightAdded"`   | `{ bookHash, id, text, note, targetIndex, color }`         |
| `"highlightChanged"` | same shape as `highlightAdded` (note or color was edited) |
| `"highlightRemoved"` | `{ bookHash, id }`                                         |

```js
mnemosyne.on("highlightAdded", function (event) {
  mnemosyne.log("New highlight in " + event.bookHash + ": " + event.text);
});
```

### `mnemosyne.log(...)`

Writes to Mnemosyne's debug log, prefixed with your plugin id. The only way
to see what a plugin is doing short of it writing a file, since there's no
UI output surface yet beyond a registered exporter's own file.

## Limits worth knowing

- Each plugin gets its own isolated JS environment — plugins can't see or
  interfere with each other.
- Any single hook call (loading your script, an event listener, an
  exporter's `format`) that runs for more than about 250ms is stopped and
  logged as a warning; write hooks that return quickly.
- A thrown exception anywhere is caught, logged, and otherwise ignored — it
  won't crash Mnemosyne or block other plugins' hooks for the same event.

## Not yet available

Commands/menu items (an action a plugin adds that runs on demand, rather
than reacting to an event) and reader CSS customization (a plugin supplying
extra stylesheet rules for EPUB/Markdown rendering) are designed but not
built yet. Network and file-system access for plugins would need a
permission model first and aren't planned for the near term.
