# ios-daisy-tests

Bring-up experimental de iOS 16.7.16 en el Xiaomi Mi A2 Lite/Redmi 6 Pro (daisy) de **4 GB**, con
un shim de arranque propio y el kernel XNU de la línea vince (CI de proxer05). :))
Solo se carga con `fastboot boot`, salvo la escritura de la raíz de iOS en `userdata`.

**Advertencia:** la raíz de iOS ocupa `userdata`, así que Android no está en el móvil.
La copia de Android está en el PC (ver docs/RESTORE_ANDROID.md). No flashear imágenes
de terceros sin verificar su hash.

## Estado (verificado en el dispositivo)

| Paso | Resultado |
|---|---|
| Shim con fastboot boot, MMU de 4 KiB, GIC, temporizador | OK |
| Lectura de la eMMC (GPT, userdata de 107069407 sectores) | OK |
| Alimentación L10/L6 por RPM y identidad del táctil (id5422) | OK |
| Kernel XNU monta la raíz de iOS desde userdata | OK (pantalla de bloqueo) |
| Táctil: desbloqueo y navegación por Ajustes, cambio de idioma | OK, lento e irregular |
| Imagen de 8 núcleos (cpus=8) | Arranca tras más de 10 minutos; no ha sido medida |
| Brillo | No implementado (sin driver de retroiluminación) |
| Aceleración GPU | No implementada |
| Tarjeta SD como almacenamiento | No: el controlador no genera comandos |

## Parches y componentes

- `patches/0001-*.patch`: corrección del mapa de memoria de la MMU para 4 GB
  (la RAM desde 0x10000000 debe ser memoria normal y ejecutable).
- `shim/`: sources del shim (MMU, GIC, temporizador, consola, lectura de eMMC).
- `tools/`: borrador del generador del árbol de dispositivo (ADT) para daisy.
- `research/device_dumps/live_fdt.dtb`: mapa de hardware del dispositivo.

El kernel XNU, el árbol de dispositivo de Apple y los IPSW no están en este repositorio:
se obtienen de los proyectos de origen (ver Créditos).

## Créditos

- arabaci0711-ux/ios-vince: base del shim y de los parches XNU para vince.
- proxer05/ios-vince: rama daisy, sonda táctil y configuración de alimentación.
- dipa-studio/ios-sweet: scripts de construcción de la raíz de iOS.
