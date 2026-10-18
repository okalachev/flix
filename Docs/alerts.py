# https://rust-lang.github.io/mdBook/format/configuration/preprocessors.html
# https://rust-lang.github.io/mdBook/for_developers/preprocessors.html

import json
import sys
import re


def transform_markdown_to_html(markdown_text):
    def replace_blockquote(match):
        tag = match.group(1).lower()
        content = match.group(2).strip().replace('\n> ', ' ')
        return f'<div class="alert alert-{tag}">{content}</div>\n'

    pattern = re.compile(r'> \[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\n>(.*?)\n?(?=(\n[^>]|\Z))', re.DOTALL)
    transformed_text = pattern.sub(replace_blockquote, markdown_text)
    return transformed_text


if __name__ == '__main__':
    if len(sys.argv) > 1:
        if sys.argv[1] == 'supports':
            sys.exit(0)

    context, book = json.load(sys.stdin)

    for section in book['sections']:
        if 'Chapter' in section:
            section['Chapter']['content'] = transform_markdown_to_html(section['Chapter']['content'])

    print(json.dumps(book))
