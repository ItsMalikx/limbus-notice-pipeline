# Limbus Notice Pipeline

A C/Python toolchain for processing, chronologically sorting, and structuring official Limbus Company game notices into a clean, searchable dataset.

This pipeline powers the [Limbus Company News Archive](https://lcna.whosmalikx.com).

## Status

This project is actively used and maintained as part of a live archive workflow. The pipeline is stable and robust, designed to handle inconsistent source data and evolving requirements.

## Overview

The pipeline ingests a master text file of Limbus Company notices (transcribed via AI OCR), intelligently matches them against official Steam announcements, and exports structured data for web deployment. It is designed to be resilient against non-standard notice formats, title discrepancies, and other "edge cases" common in raw text processing.

Core functionality includes:
- **Chronological Reconstruction:** Uses Steam metadata to accurately sort notices, even when dates are missing from the source text.
- **Sidecar Metadata:** Decouples date information from the source text, generating a clean `.tsv` file for a more robust and maintainable data flow.
- **Manual Override System:** Implements an industry-standard lookup table (`manual_overrides.csv`) to forcefully correct relationships for irregular or problematic notices.
- **Edge-Case Scanning:** Includes a dedicated utility to proactively find and flag potential mismatches, simplifying data maintenance.
- **Structured Data Generation:** Produces clean JSON for the web front-end and an XML sitemap for search engine optimization.

## The Pipeline

The data flows through a multi-stage process, separating sorting logic from data generation.

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
*   **`lc-steam-news-sorter.c`**: The primary C-based sorting and dating engine.
    - Matches notices against Steam posts using a scored substring algorithm to find the best fit.
    - Reads `manual_overrides.csv` to bypass the fuzzy matcher and enforce specific pairings.
    - Extracts official dates from Steam metadata, falling back to in-text parsing only when necessary.
    - **Outputs:**
        - `lcsn-sorted_notices.txt`: The master text, chronologically sorted and cleanly formatted.
        - `lcsn-dates.tsv`: A simple "sidecar" file mapping the new notice ID to its correct date (`ID \t YYYY-MM-DD`).

*   **`txt_to_json.py`**: Converts the processed text and date files into structured JSON.
    - Reads `lcsn-sorted_notices.txt` for content and `lcsn-dates.tsv` for dates.
    - Merges the two data sources, extracts titles, assigns keyword tags, and formats the content.
    - **Output:** `z_Output/notices.json`.

*   **`generate_sitemap.py`**: Generates a sitemap from the final JSON for SEO.
    - Reads `notices.json` and creates an XML entry for each notice.
    - **Output:** `z_Output/sitemap.xml`.

### Utility & Maintenance Scripts
*   **`scan_edge_cases.py`**: A maintenance utility to ensure data quality.
    - Compares titles in the master text against the Steam list using string similarity metrics.
    - Identifies notices with low matching confidence that may need a manual override.
    - Generates a report with suggested `^¬^` override entries to copy-paste.

*   **`z_split_notices_for_google_docs.c`**: A simple utility to split the final sorted archive into smaller `.txt` chunks for backup and external storage (e.g., Google Docs).

*   **`reverse_notices_oldest-to-newest.c`**: A utility for reversing the chronological order of a dataset and relabeling from 1 to N, useful for specific data analysis tasks.

*   **`word_frequncy_analyzer.py`**: An analysis utility for generating insights from the dataset, such as keyword frequency.

## Usage

A `Makefile` automates the entire workflow.

| Command         | Description                                                                 |
| --------------- | --------------------------------------------------------------------------- |
| `make` or `make all` | Runs the full pipeline: compile, sort, generate JSON, sitemap, and deploy.  |
| `make scan`     | Runs the `scan_edge_cases.py` utility to check for potential mismatches.      |
| `make deploy`   | Copies the final `notices.json` and `sitemap.xml` to the website repository.|
| `make clean`    | Removes all generated files (`.txt`, `.tsv`, `.json`, `.xml`, C binary).      |

### Manual Workflow
1.  `(Optional)` Run `make scan` to identify any new edge cases. Add them to `manual_overrides.csv`.
2.  Run `make` to execute the full pipeline. The final files will be generated and copied to your website directory.

## Data

#### Input Files
*   **`lcsn-MASTER.txt`**: The master archive of all notices, typically from AI OCR transcription. The order does not matter.
*   **`full_steamlist.txt`**: A reference dump of all official Steam news posts for date and title metadata.
*   **`manual_overrides.csv`**: A user-maintained lookup table to force-match irregular notice titles to their correct Steam post.
    - *Format:* `[Notice Title]^¬^[Unique Snippet from Steam Title]`

#### Key Output Files
*   **`lcsn-sorted_notices.txt`**: The clean, chronologically sorted master text.
*   **`lcsn-dates.tsv`**: The sidecar file containing notice IDs and their corresponding dates.
*   **`z_Output/notices.json`**: The final structured dataset for the website.
*   **`z_Output/sitemap.xml`**: The generated sitemap for search engines.

## License

This project is licensed under the GNU General Public License v3.0.
See the `LICENSE` file for details.
