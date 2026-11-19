# Cómo armar la entrega

Todo lo que **no** se entrega está en esta carpeta (`no_entregable/`). En la raíz del repo
quedan solo los entregables que pide el enunciado:

| Entregable del enunciado | Archivo |
|---|---|
| Código fuente sin warnings (`-Wall`) | `src/`, `Makefile` |
| Instrucciones de uso y compilación | `README.md` |
| Captura de tráfico filtrada | `TP_D_captura.pcapng` (desafío 1), `TP_D_captura2.pcapng` (desafíos 1 y 2) |
| PDF con las respuestas | `TP_D_Respuestas.pdf` |

## Armar el zip

Desde la raíz del repo (Linux o WSL):

```sh
make clean
zip -r TP_D_Grupo5.zip src Makefile README.md TP_D_captura.pcapng TP_D_captura2.pcapng TP_D_Respuestas.pdf
```

`make clean` borra los ejecutables. Igual no entrarían en el zip, porque el comando lista
explícitamente qué incluir, y por lo mismo tampoco entra el `CLAUDE.md` de la raíz (que no se
entrega: si se arma el zip de otra forma, sacarlo antes).

En Windows (PowerShell), desde la raíz del repo:

```powershell
Compress-Archive -Path src, Makefile, README.md, TP_D_captura.pcapng, TP_D_captura2.pcapng, TP_D_Respuestas.pdf -DestinationPath TP_D_Grupo5.zip
```

## Qué hay en esta carpeta

| Ruta | Qué es |
|---|---|
| `CONTEXTO.md` | Cómo trabajamos, estado y decisiones tomadas |
| `RESPUESTAS.md` / `.txt` | Borrador de las respuestas (la versión final es el PDF) |
| `informe-latex/TP_D_Respuestas.tex` | Fuente LaTeX del PDF (compilar con pdflatex dos veces, o subirlo a Overleaf) |
| `estudio-defensa/` | Material para la defensa oral: guion, preguntas y explicación de cada archivo |
| `material-clase/` | Enunciado, diapositivas de sockets y ejemplos de la cátedra |
