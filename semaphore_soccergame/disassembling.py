import subprocess
import os

def disassemble_elf(file_path, output_path):
    try:
        # Comando para desassemblar a seção de código (.text)
        with open(output_path, "w") as output_file:
            subprocess.run(
                ["objdump", "-d", file_path],
                stdout=output_file,
                stderr=subprocess.PIPE,
                text=True,
                check=True
            )
        print(f"Output do ficheiro '{file_path}' salvo em '{output_path}'.")
    except FileNotFoundError:
        print("Erro: 'objdump' não está instalado no sistema.")
    except subprocess.CalledProcessError as e:
        print(f"Erro ao processar o ficheiro ELF '{file_path}': {e.stderr}")

def main():
    while True:
        # Solicitar o nome do ficheiro ao utilizador
        elf_path = input("Insira o caminho para o ficheiro ELF (ou 0 para sair): ").strip()
        
        if elf_path == "0":
            print("Encerrando o programa.")
            break

        if not os.path.isfile(elf_path):
            print(f"Erro: O ficheiro '{elf_path}' não existe. Tente novamente.")
            continue

        # Gerar o nome do ficheiro de output com base no ficheiro de entrada
        output_path = f"{os.path.splitext(elf_path)[0]}_disassembled.txt"

        # Executar a função de desassemblar
        disassemble_elf(elf_path, output_path)

if __name__ == "__main__":
    main()
