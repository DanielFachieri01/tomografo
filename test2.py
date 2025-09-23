from tkinter import *
from tkinter import ttk, filedialog
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from PIL import Image, ImageTk  # precisa instalar o pillow -> pip install pillow
import pyvisa
import time


cor10 = "#FFFFFF"

root = Tk()
Style = ttk.Style()

class Application:
    style = ttk.Style()
    def __init__(self):
        self.root = root
        self.style = Style
        self.multimeter = None
        self.tela()
        self.botoes()
        self.fundo()
        self.iniciar_grafico()
        root.mainloop()

    def tela(self):
        self.root.title("6517b Interface")
        self.root.configure(background=cor10)
        self.root.resizable(width=True, height=True)
        self.root.geometry("1000x600")

    def conectar(self):
        try:
            # Cria o resource manager
            rm = pyvisa.ResourceManager()
            self.multimeter = rm.open_resource('GPIB0::27::INSTR')

            # Envia comandos iniciais
            self.multimeter.write(":SENSe:FUNCtion 'CURRent'")
            self.multimeter.write(":SENSe:CURRent:DC:RANGe:AUTO OFF")
            self.multimeter.write(":SYSTem:ZCHeck OFF")
            # Atualiza status na GUI
            self.status_label.config(text="Conectado Keithley 6517B", fg="green")
            # Exemplo de log local em arquivo
            with open("log.txt", "a") as log:
                log.write(":SENSe:FUNCtion 'CURRent'\n")
                log.write(":SENSe:CURRent:DC:RANGe:AUTO OFF\n")
                log.write(":SYSTem:ZCHeck OFF\n")
        except Exception as e:
                self.status_label.config(text=f"Erro: {e}", fg="red")
        
    def on_exit(self):
        # Fecha conexão se ainda estiver aberta
        if self.multimeter is not None:
            try:
                self.multimeter.close()
            except:
                pass
        self.root.destroy()  # Fecha a GUI por completo

    def botoes(self):
        self.style.configure("TButton", font=("Arial", 12), foreground="white", 
                             background="#007ACC", padding=6)
        self.style.map("TButton", foreground=[("active", "yellow")], 
                       background=[("active", "#005F99")])
        self.style.theme_use('default')

        posx0 = 0.07     # coluna 1 (labels)
        posx1 = 0.14     # coluna 2 (entries)
        posx_btn = 0.28  # coluna 3 (botões)
        posy = 0.3 
        labelw = 0.05
        entryw = 0.08
        btnw = 0.12      # largura uniforme dos botões
        h = 0.04
        offset_y = 0.06  # espaço vertical entre linhas

        # --- Labels e Entradas ---
        self.lb_v0 = Label(self.root, text='V0', bg=cor10)
        self.lb_v0.place(relx=posx0, rely=posy, relwidth=labelw, relheight=h)
        self.v0 = Entry(self.root)
        self.v0.place(relx=posx1, rely=posy, relwidth=entryw, relheight=h)

        self.lb_v1 = Label(self.root, text='V1', bg=cor10)
        self.lb_v1.place(relx=posx0, rely=posy + offset_y, relwidth=labelw, relheight=h)
        self.v1 = Entry(self.root)
        self.v1.place(relx=posx1, rely=posy + offset_y, relwidth=entryw, relheight=h)

        self.lb_passo = Label(self.root, text='Passo', bg=cor10)
        self.lb_passo.place(relx=posx0, rely=posy + 2*offset_y, relwidth=labelw, relheight=h)
        self.passo = Entry(self.root)
        self.passo.place(relx=posx1, rely=posy + 2*offset_y, relwidth=entryw, relheight=h)

        # --- Botões alinhados na coluna da direita ---
        # Start
        self.start = Button(self.root, text="Start", relief="groove", command=self.iniciar_varredura)
        self.start.place(relx=posx_btn, rely=posy, relwidth=btnw, relheight=h)
        #Stop
        self.stop = Button(self.root, text="Stop", relief="groove", command=self.parar_varredura)
        self.stop.place(relx=posx_btn, rely=posy + offset_y, relwidth=btnw, relheight=h)
        #Save
        self.save_btn = Button(self.root, text="Salvar como", relief="groove", command=self.salvar_como)
        self.save_btn.place(relx=posx_btn, rely=posy + 2*offset_y, relwidth=btnw, relheight=h)
        #Exit
        self.exit_btn = Button(self.root, text="Exit", relief="groove", command=self.root.destroy)
        self.exit_btn.place(relx=posx_btn, rely=posy + 3*offset_y, relwidth=btnw, relheight=h)
        # Botão pra conectar
        self.bt_connect = Button(self.root, text="Conectar", command=self.conectar, relief="groove")
        self.bt_connect.place(relx=0.15, rely=0.1, relwidth=0.2, relheight=0.07)
        # Label de status
        self.status_label = Label(self.root, text="Status: Desconectado", fg="red", font=("Arial", 12, "bold"))
        self.status_label.place(relx=0.1, rely=0.2, relwidth=0.3, relheight=0.07)
        # Botão Limpar
        self.bt_clear = Button(self.root, text="Limpar", command=self.limpar, relief="groove")
        self.bt_clear.place(relx=posx_btn, rely=posy + 4*offset_y, relwidth=btnw, relheight=h)

    def fundo(self):
        # Carregar a imagem

        self.bg_image = Image.open(r"C:\Users\megat\OneDrive\Área de Trabalho\IFUSP\imagens\logo_simples.jpg")
        ratio = self.bg_image.height/self.bg_image.width   
        self.bg_image = self.bg_image.resize((300, int(ratio*300)))  # redimensiona pro tamanho da janela
        self.bg_photo = ImageTk.PhotoImage(self.bg_image)

        # Label de fundo
        self.bg_label = Label(self.root, image=self.bg_photo,borderwidth = 0)
        self.bg_label.place(relx=0.02, rely=0.97, anchor="sw")  # canto inferior esquerdo

    def iniciar_grafico(self):
        self.fig, self.ax = plt.subplots(figsize=(3, 5))
        self.ax.set_title("Varredura")
        self.ax.set_xlabel("Tensão (V)")
        self.ax.set_ylabel("Corrente (nA)")
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.draw()
        self.canvas.get_tk_widget().place(relx=0.4, rely=0.0, relwidth=0.6, relheight=1.0)

    def iniciar_varredura(self):
        # Ler valores das entradas
        self.v0_val = float(self.v0.get())
        self.v1_val = float(self.v1.get())
        self.passo_val = int(self.passo.get())

        self.x_values = np.linspace(self.v0_val, self.v1_val, self.passo_val)
        # Configurações iniciais
        self.multimeter.write("*RST")
        self.multimeter.write(":SOUR:VOLT:ILIM 1e-4")   # limite de corrente (segurança)
        self.multimeter.write(":SOUR:VOLT:STAT ON")     # liga a alta tensão
        self.multimeter.write(":SENS:FUNC 'CURR'")      # mede corrente

        self.y_values = []

        # Inicializa varredura
        self.current_index = 0
        self.dynamic_x = []
        self.dynamic_y = []
        self.running = True
        self.plot_varredura()

    def plot_varredura(self):
        if not self.running or self.current_index >= len(self.x_values):
            return

        # Adiciona próximo ponto
        self.multimeter.write(f":SOUR:VOLT {self.x_values[self.current_index]}")
        self.dynamic_x.append(float(self.multimeter.query(":SOUR:VOLT?"))) #salvando o valor de tensão que o keithley tá mandando
        self.dynamic_y.append(float(self.multimeter.query(":MEAS:CURR?"))) #salvando o valor de corrente que o keithley tá medindo
        self.current_index += 1

        # Atualiza gráfico
        self.ax.clear()
        self.ax.plot(self.dynamic_x, self.dynamic_y, color='Black', marker='o', markerfacecolor = "Red")
        self.ax.set_xlabel("Tensão (V)")
        self.ax.set_ylabel("Corrente (nA)")
        self.ax.set_title("Caracterização")
        self.canvas.draw()

        # Chama novamente após 100ms
        self.root.after(100, self.plot_varredura)

    def parar_varredura(self):
        self.running = False

    def salvar_como(self):
        if not hasattr(self, "dynamic_x") or len(self.dynamic_x) == 0:
            print("Nenhum dado para salvar!")
            return

        filename = filedialog.asksaveasfilename(
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("Text files", "*.txt")],
            title="Salvar arquivo"
        )
        if filename:
            data = np.column_stack((self.dynamic_x, self.dynamic_y))
            if filename.endswith(".csv"):
                np.savetxt(filename, data, delimiter=",", header="Tensão (V), Corrente (nA)", comments="")



    def limpar(self):
        # Limpa os dados
        self.x_values = []
        self.y_values = []
        self.dynamic_x = []
        self.dynamic_y= []

        # Reseta o gráfico
        self.ax.clear()
        self.canvas.draw()
        self.iniciar_grafico()




Application()
