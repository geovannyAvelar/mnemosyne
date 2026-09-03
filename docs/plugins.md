# Writing a Mnemosyne plugin

Mnemosyne can load small JavaScript plugins (via [QuickJS](https://github.com/quickjs-ng/quickjs))
that add an export format, add an on-demand command with a simple input
form, style how a book renders, or react to reading/highlighting activity.
This document is the whole API — it's deliberately small.

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
only read what's passed into a hook and call the functions below.

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

### `mnemosyne.registerCommand(options)`

Adds an action under the **Plugins** menu, run on demand rather than in
reaction to something happening. Call this at the top level of your entry
file, same as `registerExporter`.

```js
mnemosyne.registerCommand({
  id: "word-count",             // combined with your plugin id, same as registerExporter
  label: "Word Count for This Book",
  run: function (context) {
    // context: { bookHash, title, highlights: [...] } for the open book,
    // or null when the Library tab is active instead of a book.
    if (!context) {
      mnemosyne.showMessage("Open a book first.");
      return;
    }
    const words = context.highlights.reduce((n, e) => n + e.text.split(/\s+/).length, 0);
    mnemosyne.showMessage(context.title + ": " + words + " highlighted words");
  }
});
```

`context.highlights` uses the same per-entry shape `registerExporter`'s
`entries` does. A command has no return value of its own — use
`showMessage` (or a side effect like `registerExporter`/`on`, if your
command's real job is to change what those do) to report something back.

### `mnemosyne.registerCssInjector(options)`

Adds extra CSS to how a book renders. Call this at the top level of your
entry file, same as `registerExporter`/`registerCommand`.

```js
mnemosyne.registerCssInjector({
  id: "sepia",                       // combined with your plugin id, same as registerExporter
  formats: ["epub", "mobi", "markdown"],
  css: function () {
    return "body { background: #f4ecd8; color: #3b2f2f; }";
  }
});
```

`formats` is which renderers this applies to (matched case-insensitively):
`"epub"`, `"mobi"`, or `"markdown"`. Plain text (`.txt`) isn't included —
it's rendered with no markup or document structure at all, so there's
nothing for a CSS selector to target. `css()` is called fresh every time
the relevant view re-renders (not cached, so keep it cheap) and applied
*after* Mnemosyne's own dark-mode CSS, so a plugin can override dark mode
if it wants to. Multiple injectors targeting the same format are
concatenated in registration order.

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

Writes to Mnemosyne's debug log, prefixed with your plugin id. Good for
development; a user running the released app won't see it.

### `mnemosyne.showMessage(text)`

Shows `text` to the user in a dialog. Call it from a command's
`run(context)` to report a result.

### `mnemosyne.showForm(schema)`

Shows a simple input form and returns what the user entered, or `null` if
they cancelled. Blocks until the user submits or cancels — call it from a
command's `run(context)`, same as `showMessage`.

```js
const result = mnemosyne.showForm({
  title: "Rename Tag",
  fields: [
    { id: "name", type: "text", label: "New name", default: "" },
    { id: "note", type: "multiline", label: "Note", default: "" },
    { id: "count", type: "number", label: "Count", default: 0 },
    { id: "color", type: "choice", label: "Color", options: ["Red", "Green", "Blue"], default: "Red" },
    { id: "confirmed", type: "checkbox", label: "I'm sure", default: false }
  ]
});
if (result) {
  mnemosyne.showMessage("You entered: " + result.name);
}
```

`result` is an object keyed by each field's `id`, typed to match (`count`
comes back as a number, `confirmed` as a boolean, etc.). `type` is one of
`"text"` (single line), `"multiline"`, `"number"`, `"checkbox"`, or
`"choice"` (a dropdown, populated from `options`); an unrecognized type
falls back to `"text"`. This renders with Mnemosyne's own native widgets,
not HTML — there's no way to draw a fully custom screen, only a form built
from these five field types.

## Limits worth knowing

- Each plugin gets its own isolated JS environment — plugins can't see or
  interfere with each other.
- Any single hook call (loading your script, an event listener, an
  exporter's `format`) that runs for more than about 250ms is stopped and
  logged as a warning; write hooks that return quickly. The one exception is
  `showMessage`/`showForm` — the clock pauses while the dialog is open
  waiting on the user, so taking your time filling out a form doesn't count
  against your plugin's budget.
- A thrown exception anywhere is caught, logged, and otherwise ignored — it
  won't crash Mnemosyne or block other plugins' hooks for the same event.

## Not yet available

Network and file-system access for plugins would need a permission model
first and aren't planned for the near term.
