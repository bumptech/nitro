redo-ifchange nitro.md header.html footer.html pandoc.css
pandoc -f markdown -c pandoc.css -t html5 -s --highlight-style=zenburn -B header.html -A footer.html --table-of-contents nitro.md > $3
