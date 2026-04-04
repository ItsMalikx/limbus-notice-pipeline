import json
import sys

# Usage: python generate_sitemap.py data/notices.json sitemap.xml https://lcna.whosmalikx.com

def generate_sitemap(json_path, output_path, base_url):
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            notices = json.load(f)
            
        xml_lines = [
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">',
            '  <url>',
            f'    <loc>{base_url}/</loc>',
            '    <changefreq>daily</changefreq>',
            '  </url>'
        ]
        
        for notice in notices:
            xml_lines.append('  <url>')
            xml_lines.append(f'    <loc>{base_url}/notice.html?id={notice["id"]}</loc>')
            if notice.get("date"):
                xml_lines.append(f'    <lastmod>{notice["date"]}</lastmod>')
            xml_lines.append('  </url>')
            
        xml_lines.append('</urlset>')
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(xml_lines))
            
        print(f"Sitemap successfully generated at '{output_path}' with {len(notices)} notices.")
        
    except Exception as e:
        print(f"Error generating sitemap: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python generate_sitemap.py <input_json> <output_xml> <base_url>")
        sys.exit(1)
        
    generate_sitemap(sys.argv[1], sys.argv[2], sys.argv[3])