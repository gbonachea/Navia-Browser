@echo off
REM Verifica si Python está instalado
python --version >nul 2>&1
IF ERRORLEVEL 1 (
	echo Python no está instalado. Por favor, instálalo desde https://www.python.org/downloads/
	pause
	exit /b
)

REM Verifica si pip está instalado
pip --version >nul 2>&1
IF ERRORLEVEL 1 (
	echo pip no está instalado. Instalando pip...
	python -m ensurepip
)

REM Instala PyGObject si no está instalado
python -c "import gi" 2>NUL
IF ERRORLEVEL 1 (
	echo Instalando PyGObject...
	pip install PyGObject
)

REM Ejecuta el navegador
python main.py
pause
