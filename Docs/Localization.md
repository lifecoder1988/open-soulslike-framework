# Localization

The framework's own UI text (menus, prompts, banners, notifications) is written with `LOCTEXT` in English.
Games can translate it at runtime without a localization dashboard pass:

1. Generate (or refresh) a table with every framework string:

   ```bash
   python Tools/Localization/extract_osf_text.py existing.json > osf_zh-Hans.json
   ```

   Each entry is `{"source": "<English>", "text": "<translation>"}` under its LOCTEXT namespace and key.
   Passing the existing table keeps its translations; new strings come back with an empty `text`.

2. Fill `text`, ship the file as a non-asset (`DirectoriesToAlwaysStageAsNonUFS`), and load it early, e.g. in your
   game mode's `InitGame`:

   ```cpp
   UOSFLocalization::LoadTranslationTable(FPaths::ProjectContentDir() / TEXT("Localization/osf_zh-Hans.json"), TEXT("zh-Hans"));
   ```

   Strings are registered as polyglot text data for the culture and the culture is switched. In the editor, game
   localization preview is enabled for that culture so PIE shows the translations.

The default HUD font is built at runtime from Roboto with `DroidSansFallback` as fallback typeface, so Chinese,
Japanese and Korean text renders without extra font assets. Set `AOSFHUD::TextFont` / `LargeTextFont` to use your own.
