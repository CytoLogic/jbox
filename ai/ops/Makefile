.PHONY: presentation clean

presentation: ai_ops_presentation.pdf

ai_ops_presentation.pdf: ai_ops_presentation.tex
	pdflatex ai_ops_presentation.tex
	pdflatex ai_ops_presentation.tex
	rm -f *.aux *.log *.nav *.out *.snm *.toc *.vrb

clean:
	rm -f *.aux *.log *.nav *.out *.snm *.toc *.pdf
