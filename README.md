# Packer for x86 Portable Executable applications 

## What is this?
This packer works by encrypting a specific code places, create new section that will be contain the stub for decrypting and the necessary obfuscation. \
Packer has one common stub that will be obfuscated using several methods which has adverse effects on analyzers/decompilers.

Only X86 binaries are currently supported, but in the future the ability to package x64 binaries will be added.

## Optional arguments
Packer by default works off a command line. Listed below are the arguments requried to use it.

| Argument    | Description                                                                           | Extra arguments |
| ----------- | ------------------------------------------------------------------------------------- | --------------- |
| `-oep_call` | Obfuscation of the entry point, namely the call to the required address.              |                 |
| `-adasm`    | Anti-disassembly technique, allows to break the hex-rays decompiler in particular     |                 |
| `-mba`      | Mixed Boolean Arithmetic obfuscation, inserts unwanted mathematical operations        |                 |
| `-senc`     | Includes encryption using the XOR algorithm of the required sections                  |                 |
| `-fpack`    | Encrypts the required function using the XOR algorithm, adding a stub for decryptingr | addr1, addr2    |
| `-finstr`   | Generates invalid instructions that will adversely affect analyzers/decompilers.      |                 |
| `-noaslr`   | Allows you to disable ASLR                                                            |                 |

## GUI Interface
<img width="800" height="602" alt="image" src="https://github.com/user-attachments/assets/646352aa-d4d3-4a4c-8d74-ad1d9e6852a7" />

## Packer CLI in action
![Pasted image 20250701214130](https://github.com/user-attachments/assets/c7589479-4a57-4cde-8d11-98b88a8b573b)

## Input
![Pasted image 20250701214338](https://github.com/user-attachments/assets/5145b480-4555-460c-ada2-bd2a56bec2b3)

## Output
![Pasted image 20250701214543](https://github.com/user-attachments/assets/4d9f3f37-c7cd-4153-849e-c5cd439787fd)

## Usage example
```commandline
pe-packer.exe <input.exe> <output.exe> <mutations> [flags...]

pe-packer.exe file.exe file_packed.exe 5 -mba -senc -fpack 0x401040 0x401072
```

> [!NOTE]
> The -fpack argument takes two additional arguments, the first address (start address of the function), the second address (end address of the function).

## What about .MAP parsing?
I understand how inconvenient it is to specify the addresses of the function that needs to be encrypted, the .MAP parser will be added soon, the method that is currently used will not be removed, for the reason that some people are not used to generating .MAP files at the time of compilation.

## What's next?
I am going to periodically update the functionality, add new tricks that I find, in the nearest goals there is an item added anti-debug, IAT obfuscation, anti-vm tricks and other things. You should understand that the project was created by a PE packer and there is no goal to make it an obfuscator. Perhaps you will see a separate obfuscator soon.

## Dependencies

* [AsmJit](https://github.com/asmjit/asmjit)
* [PeBliss](https://github.com/BackupGGCode/portable-executable-library)
* [Dear ImGui](https://github.com/ocornut/imgui)
