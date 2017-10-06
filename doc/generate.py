#!/bin/env python3
import jinja2

for idx in range(1, 43):
    page     = ('{0:02d}.html'.format(idx))
    loader   = jinja2.FileSystemLoader(searchpath = "src")
    env      = jinja2.Environment(loader = loader)
    template = env.get_template(page)
    with open('docs/' + page, "w+") as f:
        f.write(template.render())
