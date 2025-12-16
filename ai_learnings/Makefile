.PHONY: presentation clean

presentation: ai_strategy_presentation.pdf

ai_strategy_presentation.pdf: ai_strategy_presentation.tex
	pdflatex ai_strategy_presentation.tex
	pdflatex ai_strategy_presentation.tex

clean:
	rm -f *.aux *.log *.nav *.out *.snm *.toc *.pdf
