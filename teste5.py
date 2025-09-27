from tkinter import *
from tkinter import ttk, filedialog
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from PIL import Image, ImageTk  # pip install pillow
import pyvisa
import time


# ===================== CONFIGURAÇÕES =====================
cor10 = "#FFFFFF"

posx0 = 0.07
posx1 = 0.14
posx_btn = 0.28
posy = 0.3
labelw = 0.05
entryw = 0.08
btnw = 0.12
h = 0.055
offset_y = 0.06

multimeter = None
running = False
x_values, dynamic_x, dynamic_y = [], [], []
current_index = 0

root = Tk()

#========= funções ======================================

def main():
    global root,style
    iniciar_grafico()
    fundo()
    
    style = ttk.Style()
    style.configure("TButton", font=("Arial", 12), foreground="white",
                background="#007ACC", padding=6)
    style.map("TButton", foreground=[("active", "yellow")],
          background=[("active", "#005F99")])
    style.theme_use('default')
    root.mainloop()

def conectar():
    global multimeter
    try:
        rm = pyvisa.ResourceManager()
        multimeter = rm.open_resource('GPIB0::27::INSTR')
        multimeter.write(":SENSe:FUNCtion 'CURRent'")
        multimeter.write(":SENSe:CURRent:DC:RANGe:AUTO 0")
        multimeter.write(":SYSTem:ZCHeck ON")
        status_label.config(text="Conectado Keithley 6517B", fg="green")
    except Exception as e:
        multimeter = None
        status_label.config(text=f"Erro: {e}", fg="red")

def on_exit():
    global multimeter
    if multimeter is not None:
        try:
            multimeter.close()
        except:
            pass
    
    root.destroy()

def incrementar(entry, passo=0.1):
    try:
        valor = float(entry.get())
    except:
        valor = 0
    valor += passo
    entry.delete(0, END)
    entry.insert(0, str(round(valor,6)))

def decrementar(entry, passo=0.1):
    try:
        valor = float(entry.get())
    except:
        valor = 0
    valor -= passo
    entry.delete(0, END)
    entry.insert(0, str(round(valor,6)))


def iniciar_grafico():
    global fig, ax, canvas
    fig, ax = plt.subplots(figsize=(3,5))
    ax.set_title("Scan")
    ax.set_xlabel("ΔV GEM (V)")
    ax.set_ylabel("Current (nA)")
    canvas = FigureCanvasTkAgg(fig, master=root)
    canvas.draw()
    canvas.get_tk_widget().place(relx=0.4, rely=0.0, relwidth=0.6, relheight=1.0)

def Começar(): ## código para começar a varredura
    """
    multimeter: objeto pyvisa do Keithley (já conectado)
    v_start: tensão inicial [V]
    v_stop: tensão final [V]
    v_step_max: passo máximo de tensão [V]
    t_step: tempo de espera entre passos [s]
    dI_limit: limite de variação de corrente [A] entre um passo e outro
    """
    global multimeter, running, x_values, dynamic_x, dynamic_y, current_index
    global dI_limit, v_start, v_stop, V_step_min, V_step_max, V_prev, I_prev

    if multimeter is None:
        status_label.config(text="Erro: não conectado!", fg="red")
        return
    try:
        v_start = float(v0.get()) #V_start
        v_stop = float(v1.get()) #V_stop
        v_step = float(passo.get()) #dV_max
    except:
        status_label.config(text="Erro: valores inválidos!", fg="red")
        return
    #preparando o sistema
    multimeter.write(":SYSTem:ZCHeck OFF")
    multimeter.write(f":SYSTem:SENse:RANGe {vl_res[combo.get()]}") 
    multimeter.write(":OUTput1:STATe ON")
    multimeter.write(":SOURce:VOLTage:MCONnect ON")
    multimeter.write(":SOURce:VOLTage:RANGe MAXimum")

    V_prev = 0 #assumindo que sempre vou partir do zero
    if not(v_start == 0):
        x_values = np.arange(0,v_start+v_step,v_step)
        current_index = 0
        rampando(False) #rampa para caso o start da varredura n seja zero
    
    x_values = np.arange(v_start, v_stop + v_step, v_step) ## valores de tensão
    running = True
    dI_limit = 1e-6 # 1 micro ampere
    V_step_min = 0.1
    V_step_max = v_step #V_step é o usuário que define
    
    I_prev = float(multimeter.query(":MEAS:CURR?").split(',')[0][:-4]) #corrente no início da meidda


    dynamic_x, dynamic_y = [], []
    current_index = 0

    rampando(True)




def rampando(desenhar):
    '''
    função a ser repetida pelo root e que atualiza todos os valores e 
    indices da função anterior
    
    '''
    global I_atual, V_atual, V_step_min, V_step_max, V_prev, I_prev, running, current_index, dI_limit
    global x_values


    
    if running and current_index < len(x_values):
        V_atual = x_values[current_index] #coloca o set no index atual
        multimeter.write(f":SOUR:VOLT {V_atual}") #ajusta a tensão
        I_atual = I_atual = float(multimeter.query(":MEAS:CURR?").split(',')[0][:-4])#mede a corrente

        if desenhar:
            desenha()

        dI = abs(I_atual - I_prev) #calcula o dI 

        if dI > 1.3*dI_limit: #se o dI passar 30% do limite para tudo
            status_label.config(text="dI ultrapassou o limite", fg="green")
            running = False
            return
        
        elif dI > dI_limit: #se for maior que o limite ajuste o V_step pra algo menor
            V_step = max(V_step_min,(dI_limit/dI)*(V_atual - V_prev))

        else: #se não passar do limite pode aumentar um pouquinho 
            V_step = min(V_step_max, (V_atual-V_prev)*1.1 )

        current_index += 1 #atualiza o indice atual
        V_prev, I_prev = V_atual, I_atual
        V_atual += V_step

        root.after(1000, lambda: rampando(True)) #1s entre rampas

    else:
        running = False
        if multimeter:
            multimeter.write(":SYSTem:ZCHeck ON")
            multimeter.write(":OUTput1:STATe OFF")


def desenha():
    global V_atual, I_atual, dynamic_x, dynamic_y
    '''
    Função separada para desenhar o gráfico
    '''
    
    dynamic_x.append(V_atual) #inserir o valor atual de tensão para o gráfico
    dynamic_y.append(I_atual*1e9)
    #inserir o valor atual de corrente
    ax.clear() #limpar o gráfico anterior para atualizar
    ax.set_xlabel("Tensão (V)")
    ax.set_ylabel("Corrente (nA)") #label do y baseado na resolução pedida
    ax.plot(dynamic_x, dynamic_y, color='black', marker='o', markerfacecolor='red')#atualizar
    ax.set_title("Caracterização")
    canvas.draw()

def parar_varredura():
    global running
    running = False


def limpar():
    global x_values,dynamic_x,dynamic_y,x_values,V_step_min,V_step_max
    x_values = np.arange(x_values[current_index],0+V_step_max,V_step_max) #pegar a tensão de onde parou e descer até zero 
    rampando(desenhar = False)
    x_values = []
    dynamic_x = []
    dynamic_y = []
    ax.clear()
    canvas.draw()
    iniciar_grafico()

def salvar_como():
    if len(dynamic_x) == 0:
        print("Nenhum dado para salvar!")
        return
    filename = filedialog.asksaveasfilename(defaultextension=".csv",
                                            filetypes=[("CSV files", "*.csv"), ("Text files","*.txt")])
    if filename:
        data = np.column_stack((dynamic_x, dynamic_y))
        np.savetxt(filename, data, delimiter=",", header="Tensão (V), Corrente (nA)", comments="")

def save_graph():
    filename = filedialog.asksaveasfilename(defaultextension=".png",
                                            filetypes=[("PNG files", "*.png"), ("Todos os arquivos","*.*")])
    if filename:
        fig.savefig(filename)

def fundo():
    global bg_photo
    try:
        #.bg_image = Image.open(r"C:\Users\Hepic\Desktop\6517B\Daniel\logo_simples.jpg") ##pc do lab
        bg_image = Image.open(r"C:\Users\megat\OneDrive\Área de Trabalho\IFUSP\imagens\logo_simples.jpg") ##pc pessoal daniel
        ratio = bg_image.height / bg_image.width
        bg_image = bg_image.resize((300, int(ratio*300)))
        bg_photo = ImageTk.PhotoImage(bg_image)
        bg_label = Label(root, image=bg_photo, borderwidth=0)
        bg_label.place(relx=0.02, rely=0.97, anchor="sw")
    except Exception as e:
        print(f"Erro carregando fundo: {e}")

# ===================== WIDGETS =====================
root.title("6517B Interface")
root.configure(background=cor10)
root.geometry("1000x600")
root.protocol("WM_DELETE_WINDOW",on_exit)
root.resizable(True, True)

# Entradas V0, V1, ΔV
lb_v0 = Label(root, text="V0", bg=cor10)
lb_v0.place(relx=posx0, rely=posy, relwidth=labelw, relheight=h)
v0 = Entry(root)
v0.place(relx=posx1, rely=posy, relwidth=entryw, relheight=h)
v0.insert(0,"0.0")

lb_v1 = Label(root, text="V1", bg=cor10)
lb_v1.place(relx=posx0, rely=posy+offset_y, relwidth=labelw, relheight=h)
v1 = Entry(root)
v1.place(relx=posx1, rely=posy+offset_y, relwidth=entryw, relheight=h)
v1.insert(0,"0.0")

lb_passo = Label(root, text="ΔV", bg=cor10)
lb_passo.place(relx=posx0, rely=posy+2*offset_y, relwidth=labelw, relheight=h)
passo = Entry(root)
passo.place(relx=posx1, rely=posy+2*offset_y, relwidth=entryw, relheight=h)
passo.insert(0,"0.0")

# Botões
start = Button(root, text="Start", relief="groove", command=Começar)
start.place(relx=posx_btn, rely=posy, relwidth=btnw, relheight=h)

stop = Button(root, text="Stop", relief="groove", command=parar_varredura)
stop.place(relx=posx_btn, rely=posy+offset_y, relwidth=btnw, relheight=h)

save_btn = Button(root, text="Save Data", relief="groove", command=salvar_como)
save_btn.place(relx=posx_btn, rely=posy+2*offset_y, relwidth=btnw, relheight=h)

graph_btn = Button(root, text="Save Graph", relief="groove", command=save_graph)
graph_btn.place(relx=posx_btn, rely=posy+3*offset_y, relwidth=btnw, relheight=h)

clear_btn = Button(root, text="Clear", relief="groove", command= limpar)
clear_btn.place(relx=posx_btn, rely=posy+4*offset_y, relwidth=btnw, relheight=h)

exit_btn = Button(root, text="Exit", relief="groove", command= on_exit)
exit_btn.place(relx=posx_btn, rely=posy+5*offset_y, relwidth=btnw, relheight=h)

bt_connect = Button(root, text="Connect", relief="groove", command=conectar)
bt_connect.place(relx=0.15, rely=0.1, relwidth=0.2, relheight=0.07)

status_label = Label(root, text="Status: Disconnected", fg="red", font=("Arial",12,"bold"))
status_label.place(relx=0.1, rely=0.2, relwidth=0.3, relheight=0.07)

status_label_I = Label(root, text="I Range", bg=cor10, fg = 'black')
status_label_I.place(relx=posx0+labelw+0.01, rely=posy+4*offset_y, relwidth=btnw, relheight=h)

# Incremento/Decremento V0
btn_v0_inc = Button(root, text="+", command=lambda: incrementar(v0,10))
btn_v0_inc.place(relx=posx1+entryw, rely=posy, relwidth=0.03, relheight=h/2)
btn_v0_dec = Button(root, text="-", command=lambda: decrementar(v0,10))
btn_v0_dec.place(relx=posx1+entryw, rely=posy+h/2, relwidth=0.03, relheight=h/2)

# Incremento/Decremento V1
btn_v1_inc = Button(root, text="+", command=lambda: incrementar(v1,10))
btn_v1_inc.place(relx=posx1+entryw, rely=posy+offset_y, relwidth=0.03, relheight=h/2)
btn_v1_dec = Button(root, text="-", command=lambda: decrementar(v1,10))
btn_v1_dec.place(relx=posx1+entryw, rely=posy+offset_y+h/2, relwidth=0.03, relheight=h/2)

# Incremento/Decremento ΔV
btn_passo_inc = Button(root, text="+", command=lambda: incrementar(passo,5))
btn_passo_inc.place(relx=posx1+entryw, rely=posy+2*offset_y, relwidth=0.03, relheight=h/2)
btn_passo_dec = Button(root, text="-", command=lambda: decrementar(passo,5))
btn_passo_dec.place(relx=posx1+entryw, rely=posy+2*offset_y+h/2, relwidth=0.03, relheight=h/2)

# Seleção da resolução

opt = ["20 mA","2mA","200 uA", "20 uA","2 uA","200 nA","20 nA","2 nA","200 pA","20 pA"]
vl_res = {opt[0] : 20e-3, opt[1] : 2e-3,opt[2] : 200e-6,opt[3] :20e-6,opt[4] :2e-6,opt[5] :200e-9,opt[6] :20e-9,opt[7] :2e-9,opt[8]:200e-12,opt[9]:20e-12}

resolucao = StringVar()       
combo = ttk.Combobox(root, textvariable=resolucao, values=opt, state="readonly")
combo.set(opt[0])
combo.place(relx=posx1, rely=posy+5*offset_y, relwidth=entryw+0.03, relheight=h)

#
main()