# HorseGaitFix (SIN COMPILAR NI PROBAR)
Plugin SKSE independiente (Skyrim SE 1.6.1170, SKSE 2.2.8). No modifica STR.
Cada 50 ms mide la velocidad real de cada caballo/animal cargado; si el grafo tiene `Speed` ~0 pero el actor se mueve, escribe esa velocidad.
Compilar (Windows): `xmake f -m release && xmake`. Instalar: `SKSE/Plugins/HorseGaitFix.dll` como mod en MO2. Quitar: desactivar el mod.
Riesgos: `ActorTypeAnimal` incluye perros/cabras; la API exacta de CommonLibSSE-NG puede variar; el hilo toca actores vía AddTask (hilo del juego).
Pruebas: ver `Speed` con `getav`/consola no sirve; comprobar visualmente al seguir a un jinete remoto.
