# ios-daisy (En versión alpha)

(Dame creditos si haces uso del REPO porfis, gracias <3)

Xiaomi Redmi 6 Pro 64/4 (Daisy / Qualcomm MSM8953 Snapdragon 625), targeting iOS 16

The actual REPO where i'm working on updates is on private (for now) :P

Pruebas de bring-up de un shim de arranque para el **Xiaomi Mi A2 Lite (daisy, MSM8953)** de **4 GB**,
con la intención de hacer funcionar iOS 16 mediante un kernel XNU con un shim de arranque propio y el kernel XNU de la línea vince (CI de proxer05). :))
**Estado: experimental. No es un sistema operativo estable ni usable, puede dejar tu móvil como un ladrillo :P.**

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

## Qué NO está en este R.E.P.O

- Copias de particiones (`userdata`, `gpt`, `ssd`, `keystore`, `boot`...). Son MIOSSS ;P
- IPSW de iOS ni el árbol de dispositivo de Apple (`DeviceTree.d22ap`). Hay que obtenerlos del propio IPSW de Apple.
- Binarios del kernel XNU compilados.

## Créditos

- arabaci0711-ux/ios-vince: base del shim y de los parches XNU para vince.
- proxer05/ios-vince: rama daisy, sonda táctil y configuración de alimentación.
- dipa-studio/ios-sweet: scripts de construcción de la raíz de iOS.
- Dumped files from https://github.com/frost2k5-org/xiaomi_daisy_sprout_dump
- postmarketOS y msm8953-mainline: referencia de hardware.
- Apple: XNU de código abierto.
