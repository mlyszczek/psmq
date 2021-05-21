EXTRA_DIST = custom.css footer.in header.in index.in index.md post-process.sh

www:
	kursg -o out
	./post-process.sh
	chmod a+rX out -R

clean:
	rm -f downloads.html
	rm -f -r out
	rm -f -r manuals
