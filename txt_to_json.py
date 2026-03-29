import json
import re
import argparse

# Run using: python txt_to_json.py lcsn-sorted_notices.txt notices.json full_steamlist.txt

def parse_month(m_str):
    months = ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"]
    m_lower = m_str.lower()[:3]
    if m_lower in months:
        return months.index(m_lower) + 1
    return 1

def parse_steam_blocks(text):
    """
    Scans raw text for Steam post metadata to build the primary repository of dates.
    Relies on the "Share" footer to segment individual steam listings.
    """
    steam_posts = []
    blocks = text.split("Share")
    
    first_year_match = re.search(r'\b(202[3-9])\b', text)
    current_year = int(first_year_match.group(1)) if first_year_match else 2026
    
    for block in blocks:
        if not block.strip():
            continue
            
        date_match = re.search(r'\b(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun),\s+([A-Z][a-z]{2,8})\s+(\d{1,2})(?:st|nd|rd|th)?(?:,\s*(\d{4}))?\b', block, re.IGNORECASE)
        
        if date_match:
            m_str = date_match.group(1)
            d_str = date_match.group(2)
            y_str = date_match.group(3)
            
            m = parse_month(m_str)
            d = int(d_str)
            
            if y_str:
                y = int(y_str)
                current_year = y
            else:
                y = current_year
                
            normalized = re.sub(r'[^\w]', '', block.lower())
            
            post_words = set(re.findall(r'\w+', block.lower()))
            post_words = {w for w in post_words if len(w) > 2}
            
            steam_posts.append({
                'date': (y, m, d),
                'normalized': normalized,
                'word_set': post_words
            })
            
    return steam_posts

def get_steam_date(title, content, steam_posts):
    """
    Matches a notice against the steam_posts corpus using a hybrid approach
    (Strict Substring & Deep Word Overlap)
    """
    if not steam_posts:
        return None
        
    notice_text = (title + " " + content).lower()
    notice_words = set(re.findall(r'\w+', notice_text))
    notice_words = {w for w in notice_words if len(w) > 2}
    
    title_norm = re.sub(r'[^\w]', '', title.lower())
    notice_norm_150 = re.sub(r'[^\w]', '', notice_text)[:150]
    
    best_match = None
    best_overlap = 0
    best_is_direct = False
    
    for post in steam_posts:
        is_direct = False
        post_norm = post['normalized']
        
        if len(title_norm) > 8 and title_norm in post_norm:
            is_direct = True
        elif len(notice_norm_150) > 20 and notice_norm_150 in post_norm:
            is_direct = True
            
        overlap = len(notice_words.intersection(post['word_set']))
        
        if is_direct:
            overlap += 1000
            
        if overlap > best_overlap:
            best_overlap = overlap
            best_match = post
            best_is_direct = is_direct

    if best_match:
        if best_is_direct:
            return best_match['date']
        if best_overlap >= 12 or (len(notice_words) > 0 and best_overlap >= max(5, len(notice_words) * 0.4)):
            return best_match['date']
            
    return None

def extract_date_fallback(title, content, current_year):
    """
    Fallback native date extractor if Steam cross-referencing fails.
    """
    year_match = re.search(r'\b(202[3-9])\b', title + " " + content)
    if year_match:
        current_year = int(year_match.group(1))

    patterns = [
        (r'(202[3-9])\.\s*(0?[1-9]|1[0-2])\.\s*(0?[1-9]|[12]\d|3[01])', lambda m: (int(m.group(1)), int(m.group(2)), int(m.group(3)))),
        (r'(0?[1-9]|1[0-2])/(0?[1-9]|[12]\d|3[01])/(202[3-9])', lambda m: (int(m.group(3)), int(m.group(1)), int(m.group(2)))),
        (r'(202[3-9]),?\s+(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]*\.?\s+(\d{1,2})', lambda m: (int(m.group(1)), parse_month(m.group(2)), int(m.group(3)))),
        (r'(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]*\.?\s+(\d{1,2})(?:st|nd|rd|th)?,\s+(202[3-9])', lambda m: (int(m.group(3)), parse_month(m.group(1)), int(m.group(2)))),
        (r'(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]*\.?\s+(\d{1,2})(?:st|nd|rd|th)?', lambda m: (current_year, parse_month(m.group(1)), int(m.group(2)))),
        (r'(0?[1-9]|1[0-2])\.(0?[1-9]|[12]\d|3[01])\s*\([A-Za-z]+\)', lambda m: (current_year, int(m.group(1)), int(m.group(2))))
    ]

    def find_best_date(text):
        best_match = None
        best_pos = len(text)
        for pat, extractor in patterns:
            for m in re.finditer(pat, text, re.IGNORECASE):
                if m.start() < best_pos:
                    best_pos = m.start()
                    best_match = extractor(m)
        return best_match

    match = find_best_date(title)
    if match:
        return match[0], match[1], match[2], current_year

    match = find_best_date(content)
    if match:
        return match[0], match[1], match[2], current_year

    return None, None, None, current_year

def assign_tags(title, content):
    text = (title + " " + content).lower()
    tags = []

    if any(w in text for w in ['railway', 'refraction', 'station']):
        tags.append("Refraction Railway")
    if any(w in text for w in ['walpurgis']):
        tags.append("Walpurgis Night")
    if any(w in text for w in ['mirror dungeon', 'mirror of the']):
        tags.append("Mirror Dungeon")
    if any(w in text for w in ['update', 'maintenance', 'scheduled', 'hotfix']):
        tags.append("Update")
    if any(w in text for w in ['issue', 'fixed', 'error', 'bug', 'known issues']):
        tags.append("Bug Fix")
    if any(w in text for w in ['compensation', 'apologize', 'apology']):
        tags.append("Notice")

    if not tags:
        tags.append("Announcement")

    return tags[:3]

def convert_txt_to_json(input_file, output_file, steam_file=None):
    with open(input_file, "r", encoding="utf-8") as f:
        text = f.read()

    steam_posts = []
    if steam_file:
        try:
            with open(steam_file, "r", encoding="utf-8") as sf:
                steam_text = sf.read()
                steam_posts = parse_steam_blocks(steam_text)
                print(f"Loaded {len(steam_posts)} Steam posts for date matching.")
        except FileNotFoundError:
            print(f"Warning: Steam file '{steam_file}' not found. Will rely entirely on native date extraction.")

    notices = []

    pattern = re.compile(
        r"Writing Sample\s+(\d+):\s*\n([\s\S]*?)(?=\nWriting Sample\s+\d+:\s*\n|\Z)",
        re.MULTILINE
    )

    current_year = 2023 
    years = re.findall(r'\b(202[3-9])\b', text)
    if years:
        current_year = max(int(y) for y in years)

    steam_match_count = 0
    native_match_count = 0
    omitted_count = 0

    for match in pattern.finditer(text):
        notice_id = int(match.group(1))
        full_block = match.group(2).strip()

        if full_block.startswith("‘") and full_block.endswith("’"):
            full_block = full_block[1:-1].strip()
        elif full_block.startswith("'") and full_block.endswith("'"):
            full_block = full_block[1:-1].strip()
        elif full_block.startswith('"') and full_block.endswith('"'):
            full_block = full_block[1:-1].strip()

        lines = full_block.splitlines()
        while lines and not lines[0].strip():
            lines.pop(0)

        if not lines:
            continue

        title = lines[0].strip()
        content_body = "\n".join(lines[1:]).strip()

        y, m, d = None, None, None

        steam_date = get_steam_date(title, content_body, steam_posts)
        if steam_date:
            y, m, d = steam_date
            steam_match_count += 1
        else:
            y, m, d, current_year = extract_date_fallback(title, content_body, current_year)
            if y and m and d:
                native_match_count += 1
            else:
                omitted_count += 1

        date_str = f"{y}-{m:02d}-{d:02d}" if y and m and d else ""
        tags = assign_tags(title, content_body)

        notices.append({
            "id": notice_id,
            "title": title,
            "date": date_str,
            "tags": tags,
            "content": content_body
        })

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(notices, f, indent=2, ensure_ascii=False)

    print(f"\nSuccessfully converted {len(notices)} notices to '{output_file}'")
    print(f" - Dates accurately assigned via Steam Match: {steam_match_count}")
    print(f" - Dates assigned via Native Text Fallback: {native_match_count}")
    if omitted_count > 0:
        print(f" - Notices with NO date found: {omitted_count}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert Limbus Company TXT notices to structured JSON.")
    parser.add_argument("input", help="The input TXT file containing Writing Samples (e.g., lcsn-sorted_notices.txt)")
    parser.add_argument("output", nargs="?", default="notices.json", help="The output JSON file (default: notices.json)")
    parser.add_argument("steam", nargs="?", default=None, help="The Steam news list TXT file for accurate date matching (optional)")

    args = parser.parse_args()
    
    convert_txt_to_json(args.input, args.output, args.steam)