# Limbus Notice Pipeline

A C/Python toolchain I built to process, chronologically sort, and structure official Limbus Company game notices into a clean, searchable dataset.

This pipeline powers my Limbus Company News Archive: [lcna.whosmalikx.com](https://lcna.whosmalikx.com)

## How It Works

This pipeline processes a master text file of Limbus Company notices, which are often transcribed via AI OCR and lack reliable date metadata. It intelligently matches them against official Steam announcements to reconstruct the correct chronological order and exports structured data for web deployment.

To handle inconsistencies in the source data, I implemented a few key features:
-   **Sidecar Metadata:** Instead of embedding dates into the text, the C sorter generates a clean `.tsv` file. This decouples the content from its metadata, creating a more robust and maintainable data flow.
-   **Manual Override System:** For irregular notices with mismatched titles, I use a simple CSV lookup table (`manual_overrides.csv`) to forcefully create the correct relationship, bypassing the automated matching logic.
-   **Edge-Case Scanning:** I wrote a dedicated Python utility that proactively scans for potential mismatches between the master file and the Steam list, making it easy to spot and fix errors.

## The Pipeline

The data flows through a multi-stage process, separating the complex sorting logic from the final data generation.

```
lcsn-MASTER.txt + full_steamlist.txt + manual_overrides.csv
                    │
                    ↓
           lc-steam-news-sorter.c
                    │
        ┌───────────┴───────────┐
        ↓                       ↓
lcsn-sorted_notices.txt   lcsn-dates.tsv
        │                       │
        └───────────┬───────────┘
                    ↓
              txt_to_json.py
                    │
                    ↓
z_Output/notices.json ───────> generate_sitemap.py ───> z_Output/sitemap.xml
```

## Components

### Core Pipeline
*   **`lc-steam-news-sorter.c`**: This is the primary C-based sorting and dating engine. It matches notices to Steam posts, reads manual overrides, extracts dates, and outputs two key files:
    - `lcsn-sorted_notices.txt`: The master text, sorted chronologically and cleanly formatted.
    - `lcsn-dates.tsv`: A sidecar file mapping each new notice ID to its correct date (`ID \t YYYY-MM-DD`).

*   **`txt_to_json.py`**: This Python script converts the processed text and date files into structured JSON for my website. It reads both the `.txt` and `.tsv`, merges them, extracts titles, assigns tags, and formats the content.

*   **`generate_sitemap.py`**: This script generates an XML sitemap from the final `notices.json` file to improve search engine optimization for the archive.

### Utility & Maintenance Scripts
*   **`scan_edge_cases.py`**: A maintenance script I run to ensure data quality. It compares titles in my master text against the Steam list and flags any with low matching confidence that might need a manual override.

*   **`z_split_notices_for_google_docs.c`**: A simple utility to split the final sorted archive into smaller `.txt` chunks for backup on Google Docs.

*   **`reverse_notices_oldest-to-newest.c`**: A utility for reversing the chronological order of a dataset, which is useful for certain data analysis tasks.

*   **`word_frequncy_analyzer.py`**: An analysis script for generating insights from the dataset, such as keyword frequency.

## Usage

I've automated the entire workflow using a `Makefile`.

| Command         | Description                                                                 |
| --------------- | --------------------------------------------------------------------------- |
| `make` or `make all` | Runs the full pipeline: compiles the C code, sorts the data, generates the JSON and sitemap, and deploys the files. |
| `make scan`     | Runs the `scan_edge_cases.py` utility to help me check for any new mismatches. |
| `make deploy`   | Manually copies the final `notices.json` and `sitemap.xml` to the website repository.|

### My Workflow
1.  First, I update my `lcsn-MASTER.txt` and `full_steamlist.txt` with the latest notices.
2.  Next, I run `make scan`. If it finds any irregular notices, I add an entry for them in `manual_overrides.csv` to ensure they're matched correctly.
3.  Finally, I just run `make`. This executes the entire pipeline from start to finish.

The `deploy` step in the `Makefile` is configured for my project. It automatically copies the generated `notices.json` and `sitemap.xml` directly into my website's repository. If you are adapting this pipeline for your own use, you'll want to change the `SITE_DIR` variable at the top of the `Makefile` to point to your project's directory.

## Data

#### Input Files
*   **`lcsn-MASTER.txt`**: The master archive of all notices. The order of notices in this file does not matter.
*   **`full_steamlist.txt`**: A reference dump of all official Steam news posts, used for date and title metadata.
*   **`manual_overrides.csv`**: My user-maintained lookup table to force-match irregular notices. The format is `[Notice Title]^¬^[Unique Snippet from Steam Title]`.

#### Key Output Files
*   **`lcsn-sorted_notices.txt`**: The clean, chronologically sorted master text.
*   **`lcsn-dates.tsv`**: The sidecar file containing notice IDs and their corresponding dates.
*   **`z_Output/notices.json`**: The final structured dataset used by the website.
*   **`z_Output/sitemap.xml`**: The generated sitemap for search engines.

## License

This project is licensed under the GNU General Public License v3.0.
See the `LICENSE` file for details.
