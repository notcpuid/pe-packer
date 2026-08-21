<div align="center">
<img width="1408" height="768" alt="image" src="github_images/logo_large.png" />
</div>

## What is this?

`pe-packer` takes a Windows Portable Executable (PE) file, makes a new executable section with a runtime stub, and applies a set of optional obfuscation/encryption techniques.

The stub can be mutated based on a **single “obfuscation level” (1–10)** so that the parameter reliably controls complexity/size of the emitted stub.

Both **x86 (PE32)** and **x64 (PE32+)** targets are supported.

> [!IMPORTANT]
> This project is for research/education. Use responsibly and only on binaries you own or have explicit permission to modify.

## Build

Open `pe-packer.sln` in Visual Studio and build **Release x64**.

The packer itself is a x64 application, but it can pack both x86 and x64 PE targets.

## Usage (CLI)

```commandline
pe-packer.exe <input.exe> <output.exe> <level 1..10> [flags...]
```

Examples:

```commandline
pe-packer.exe input.exe output.exe 5 -mba -senc
pe-packer.exe input.exe output.exe 7 -oep_call -adasm -finstr
pe-packer.exe input.exe output.exe 4 -fpack 0x401040 0x401072
```

### Obfuscation level

-   **Level range**: `1..10`
-   **Meaning**: controls the stub mutation profile (number of passes, MBA weight, inner loop sizes, fake bytes ranges, etc.)

### Optional flags

| Argument    | Description                                                                         | Extra arguments       |
| ----------- | ----------------------------------------------------------------------------------- | --------------------- |
| `-oep_call` | Obfuscates the transfer to the original entry point using an indirect call.         |                       |
| `-adasm`    | Anti-disassembly trick(s) intended to confuse decompilers (e.g. Hex-Rays).          |                       |
| `-mba`      | Mixed Boolean Arithmetic blocks inside the stub.                                    |                       |
| `-senc`     | Encrypt selected sections with XOR and emit a runtime decrypt loop.                 |                       |
| `-fpack`    | Encrypt a function range with XOR and emit a runtime decrypt loop.                  | `addr_start addr_end` |
| `-finstr`   | Emit random bytes (“fake instructions”) to harm linear sweep / analysis heuristics. |                       |
| `-noaslr`   | Clear the `DYNAMIC_BASE` flag (disable ASLR) in the output image.                   |                       |

> [!NOTE]
> `-fpack` takes two additional arguments: start and end address of the function range.

## GUI Interface

<img width="800" height="600" alt="image" src="https://github.com/user-attachments/assets/5737a068-e21f-41c7-b93e-b640283133c8" />

### Obfuscation level mapping

The GUI slider is `1..100` and is mapped to **level 1..10**:

-   `1..10` → level 1
-   `11..20` → level 2
-   ...
-   `91..100` → level 10

## Packer CLI in action

![Pasted image 20250701214130](https://github.com/user-attachments/assets/c7589479-4a57-4cde-8d11-98b88a8b573b)

## Input

![Pasted image 20250701214338](https://github.com/user-attachments/assets/5145b480-4555-460c-ada2-bd2a56bec2b3)

## Output

<img width="762" height="641" alt="image" src="https://github.com/user-attachments/assets/ebc61519-524d-45d6-a859-fc54c96a24e6" />

## CFG / ASLR notes

-   **CFG (`/guard:cf`)**: if the input image has CFG enabled, the packer patches the Guard CF (GFIDS) table so that the put stub entry point is a valid indirect-call target.  
    If `-oep_call` is enabled, the original OEP is also registered as a valid CFG target (so the indirect transfer remains valid).
-   **ASLR**: `-noaslr` clears the flag; otherwise ASLR is preserved.

## What about .MAP parsing?

Specifying function ranges for `-fpack` manually is inconvenient. A `.MAP` parser / helper will likely be added in the future, but manual address mode will remain available.

## What's next?

Planned improvements may include: anti-debug, IAT obfuscation, anti-VM tricks, and more PE hardening / analysis tricks.

## Dependencies

-   [Dear ImGui](https://github.com/ocornut/imgui)
-   [x86-x64 Emitter](https://github.com/D7EAD/mkPIVM)
