from tkinter import *
from tkinter import ttk, filedialog,simpledialog
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import threading
from PIL import Image, ImageTk  
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

root = Tk()
multimeter = None
stop_event = None
resultados_scan = []
resultados_monitor = []

# ========================== inicialização ==========================

# Variável global
session_name = None

def pedir_nome_sessao():
    global session_name
    temp_root = Tk()
    temp_root.withdraw()  # Esconde janela principal

    session_name = simpledialog.askstring("Nome da Sessão", "Digite o nome da sessão:", parent=temp_root)
    if not session_name:
        session_name = "default_session"

    temp_root.destroy()

# Chame a função antes de inicializar o root principal
pedir_nome_sessao()


def conectar():
    global multimeter
    try:
        rm = pyvisa.ResourceManager()
        multimeter = rm.open_resource('GPIB0::27::INSTR')
        escrever_log(f'conexão iniciada com "open_resource("GPIB0::27::INSTR")" em {time.ctime()}')

        multimeter.write(":SYSTem:ZCHeck ON")
        escrever_log(":SYSTem:ZCHeck ON")

        multimeter.write(":SENSe:FUNCtion 'CURRent'")
        escrever_log("Comando ':SENSe:FUNCtion 'CURRent'")
  
        multimeter.write(":SENSe:CURRent[:DC]RANGe:AUTO OFF")
        escrever_log("Comando ':SENSe:CURRent[:DC]RANGe:AUTO OFF'")

        multimeter.write(":SENSe:CURRent[:DC]:RANGe: 0.001")
        escrever_log("Comando ':SENSe:CURRent[:DC]:RANGe: 0.001'")
        escrever_log(f"Resposta:{multimeter.read()} ")
        status_label.config(text="Conectado Keithley 6517B", fg="green")

        multimeter.write(":OUTput1:STATe ON")
        escrever_log("Comando : ':OUTput1:STATe ON'")
        multimeter.write(":SOURce:VOLTage:MCONnect ON")
        escrever_log("Comando ':SOURce:VOLTage:MCONnect ON'")
        multimeter.write(":SOURce:VOLTage:RANGe MAXimum")
        escrever_log("Comando: ':SOURce:VOLTage:RANGe MAXimum'")


    except Exception as e:
        multimeter = None
        status_label.config(text=f"Erro: {e}", fg="red")
        escrever_log(f"Não conectou com o keithley em {time.ctime()}")

def iniciar_grafico():
    global fig, ax_scan, ax_monitor, canvas
    fig, (ax_scan, ax_monitor) = plt.subplots(2,1, figsize=(6,6))
    fig.tight_layout(pad=3.0)

    ax_scan.set_title("Caracterização (Varredura)")
    ax_scan.set_xlabel("Tensão (V)")
    ax_scan.set_ylabel("Corrente (nA)")

    ax_monitor.set_title("Monitoramento")
    ax_monitor.set_xlabel("Tempo (s)")
    ax_monitor.set_ylabel("Corrente (nA)")

    canvas = FigureCanvasTkAgg(fig, master=root)
    canvas.draw()
    canvas.get_tk_widget().place(relx=0.45, rely=0.0, relwidth=0.5, relheight=1.0)

    fundo()

def meas_current():
    return float(multimeter.query(":MEAS:CURR?"))

def out_v(v):
    multimeter.write(f":SOUR:VOLT {v}")

def fundo():
    global bg_photo
    try:
        #bg_image = Image.open(r"C:\Users\Hepic\Desktop\6517B\Daniel\logo_simples.jpg") ##pc do lab
        bg_image = Image.open(r"C:\Users\megat\OneDrive\Área de Trabalho\IFUSP\imagens\logo_simples.jpg") ##pc pessoal daniel
        ratio = bg_image.height / bg_image.width
        bg_image = bg_image.resize((300, int(ratio*300)))
        bg_photo = ImageTk.PhotoImage(bg_image)
        bg_label = Label(root, image=bg_photo, borderwidth=0)
        bg_label.place(relx=0.02, rely=0.97, anchor="sw")
    except Exception as e:
        print(f"Erro carregando fundo: {e}")

# ===================== scan =====================
def scan(stop_event):
    global resultados_scan
    escrever_log(f"função Scan iniciada em {time.ctime()}")
    resultados_scan = []
    v_ini = 0.0
    v_fim = float(v1.get())
    dv = float(passo.get())
    x_values = list(np.arange(v_ini, v_fim+dv, dv))

    limite_delta = 1e-8
    timeout = 10

    for v in x_values:
        if stop_event.is_set():
            escrever_log("Scan Interrompido pelo usuário.")
            break

        out_v(v)
        escrever_log(f"Scan Aplicado V={v} V")

        t0 = time.time()
        i_past = meas_current()

        while True:
            if stop_event.is_set():
                return resultados_scan
            time.sleep(0.2)
            i = meas_current()
            dI = abs(i - i_past)

            if dI < limite_delta:
                break
            if time.time() - t0 > timeout:
                break
            i_past = i

        resultados_scan.append((v,i))
        desenha_scan(resultados_scan)

# ===================== monitoramento =====================
def monitorar(stop_event):
    global resultados_monitor
    escrever_log(f'monitor iniciando em {time.ctime()}')
    resultados_monitor = []
    t0 = time.time()
    i_past = meas_current()
    dI_lim = 20e-6

    while not stop_event.is_set():
        time.sleep(0.1)
        i = meas_current()
        t1 = time.time()
        resultados_monitor.append((t1-t0, i))
        desenha_monitor(resultados_monitor)

        if abs(i - i_past) > dI_lim:
            stop_event.set()
            escrever_log(f'dI máximo atingido = {dI} A em {time.ctime()}')
            break
        i_past = i

#=============== plotar a média no monitoramento ===========
def plot_media():
    global resultados_monitor
    if not resultados_monitor:
        escrever_log("Nenhum dado no monitor para calcular média!")
        return
    
    try:
        t0 = float(v0_mean.get())
        t1 = float(v1_mean.get())
    except ValueError:
        escrever_log("Intervalo inválido! Insira números válidos em t0 e t1.")
        
        return

    data = np.array(resultados_monitor) 
    
    mask = (data[:,0] >= t0) & (data[:,0] <= t1)
    if not np.any(mask):
        escrever_log("Nenhum dado no intervalo selecionado!")
        return

    data_intervalo = data[mask]
    media_corrente = np.mean(data_intervalo[:,1])

    # Plota linha média no gráfico de monitoramento
    ax_monitor.axhline(media_corrente, color='red', linestyle='--',
                       label=f"Média ({t0:.2f}s-{t1:.2f}s) = {media_corrente*1e9:.2f} nA")
    ax_monitor.legend()
    canvas.draw()
    escrever_log(f"gráfico de média plotado em {time.ctime()}")


# ===================== DESENHO =====================
def desenha_scan(resultados):
    if not resultados: return
    ax_scan.clear()
    ax_scan.set_xlabel("Tensão (V)")
    ax_scan.set_ylabel("Corrente (nA)")
    data = np.array(resultados)
    ax_scan.plot(data[:,0], data[:,1]*1e9, color='black', marker='o', markerfacecolor='red')
    ax_scan.set_title("Caracterização")
    canvas.draw()

def desenha_monitor(resultados):
    if not resultados: return
    ax_monitor.clear()
    ax_monitor.set_xlabel("Tempo (s)")
    ax_monitor.set_ylabel("Corrente (nA)")
    data = np.array(resultados)
    ax_monitor.plot(data[:,0], data[:,1]*1e9, color='black', marker='o', markerfacecolor='red')
    ax_monitor.set_title("Monitoramento")
    canvas.draw()

# ===================== start/stop =====================
def iniciar_medicao():
    escrever_log(f"Tomada de dados iniciada em {time.ctime()}")
    global stop_event
    stop_event = threading.Event()
    t_scan = threading.Thread(target=scan, args=(stop_event,))
    t_monitor = threading.Thread(target=monitorar, args=(stop_event,))
    t_scan.start()
    t_monitor.start()

def stop():
    if stop_event:
        stop_event.set()
        escrever_log(f'botão de stop pressionado em {time.ctime()}')

# ===================== salvar dados =====================
def salvar_como():
    global resultados_scan, resultados_monitor, log
    nome = session_name
    if not nome:
        print("Insira um nome de arquivo válido!")
        escrever_log("Insira um nome de arquivo válido!")
        return

    if not resultados_scan and not resultados_monitor:
        print("Nenhum dado para salvar!")
        escrever_log("Nenhum dado para salvar!")
        return

    # Scan
    if resultados_scan:
        data_scan = np.array(resultados_scan)
        np.savetxt(f"{nome}_scan.csv", data_scan, delimiter=",",
                   header="Tensão (V), Corrente (A)", comments="")
        print(f"Scan salvo em {nome}_scan.csv")
        escrever_log(f"Scan salvo em {nome}_scan.csv")

    # Monitor
    if resultados_monitor:
        data_monitor = np.array(resultados_monitor)
        np.savetxt(f"{nome}_monitor.csv", data_monitor, delimiter=",",
                   header="Tempo (s), Corrente (A)", comments="")
        print(f"Monitor salvo em {nome}_monitor.csv")
        escrever_log(f"Monitor salvo em {nome}_monitor.csv")
    
# ===================== salvar gráfico =====================
def save_graph(fig):
    nome = session_name
    if not nome:
        print("Insira um nome de arquivo válido!")
        return
    fig.savefig(f"{nome}.png", dpi=300)
    print(f"Gráfico salvo em {nome}.png")
    escrever_log(f"Gráfico salvo em {nome}.png")
# ===================== clear =====================
def limpar():
    global resultados_scan, resultados_monitor
    resultados_scan = []
    resultados_monitor = []
    ax_scan.clear()
    ax_monitor.clear()
    escrever_log(f'Botão de "clear" pressionado em {time.ctime()}')
    iniciar_grafico()

# ======= fechar a janela e quebrar a conexão ========
def on_exit():
    if multimeter:
        try: multimeter.close()
        except: pass
    root.destroy()

#========= criar o arquivo de log ====================

# Cria o log no início
def criar_log():
    nome = session_name
    if not nome:
        nome = "default_log"
    log_file = f"{nome}.log"
    with open(log_file, "w") as f:
        f.write(f"Log iniciado em {time.ctime()}\n")
    return log_file

log_file = criar_log()

# Função auxiliar para escrever no log
def escrever_log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(log_file, "a") as f:
        f.write(f"[{timestamp}] {msg}\n")
    print(msg)  # mantém no console também


# ===================== WIDGETS ======================
root.title("6517B Interface")
root.configure(background=cor10)
root.geometry("1000x600")
root.protocol("WM_DELETE_WINDOW",on_exit)
root.resizable(True, True)

# Entradas
lb_v1 = Label(root, text="Target V", bg=cor10)
lb_v1.place(relx=posx0, rely=posy, relwidth=labelw, relheight=h)
v1 = Entry(root)
v1.place(relx=posx1, rely=posy, relwidth=entryw, relheight=h)
v1.insert(0,"1.0")

lb_passo = Label(root, text="ΔV", bg=cor10)
lb_passo.place(relx=posx0, rely=posy+2*offset_y, relwidth=labelw, relheight=h)
passo = Entry(root)
passo.place(relx=posx1, rely=posy+2*offset_y, relwidth=entryw, relheight=h)
passo.insert(0,"0.1")

# Botões
start_scan = Button(root, text="Start", relief="groove", command=iniciar_medicao)
start_scan.place(relx=posx_btn, rely=posy, relwidth=btnw, relheight=h)

stop_scan = Button(root, text="Stop", relief="groove", command=stop)
stop_scan.place(relx=posx_btn, rely=posy+offset_y, relwidth=btnw, relheight=h)

save_btn = Button(root, text="Save Data", relief="groove", command=salvar_como)
save_btn.place(relx=posx_btn, rely=posy+2*offset_y, relwidth=btnw, relheight=h)

graph_btn = Button(root, text="Save Graph", relief="groove", command=lambda: save_graph(fig))
graph_btn.place(relx=posx_btn, rely=posy+3*offset_y, relwidth=btnw, relheight=h)

clear_btn = Button(root, text="Clear", relief="groove", command=limpar)
clear_btn.place(relx=posx_btn, rely=posy+4*offset_y, relwidth=btnw, relheight=h)

exit_btn = Button(root, text="Exit", relief="groove", command=on_exit)
exit_btn.place(relx=posx_btn, rely=posy+5*offset_y, relwidth=btnw, relheight=h)

bt_connect = Button(root, text="Connect", relief="groove", command=conectar)
bt_connect.place(relx=0.1+0.15/2, rely=0.2-0.07, relwidth=0.15, relheight=0.05)

status_label = Label(root, text="Status: Disconnected", fg="red", font=("Arial",12,"bold"))
status_label.place(relx=0.1, rely=0.2, relwidth=0.3, relheight=0.07)

btn_media = Button(root,text="Mean",relief='groove', command = plot_media)
btn_media.place(relx=posx1, rely=posy+5*offset_y, relwidth=labelw, relheight=h)

lb_v0_mean = Label(root, text="t0", bg=cor10)
lb_v0_mean.place(relx=posx0, rely=posy+3*offset_y, relwidth=labelw, relheight=h)
v0_mean = Entry(root)
v0_mean.place(relx=posx1, rely=posy+3*offset_y, relwidth=entryw, relheight=h)
v0_mean.insert(0,"0.0")

lb_v1_mean = Label(root, text="t1", bg=cor10)
lb_v1_mean.place(relx=posx0, rely=posy+4*offset_y, relwidth=labelw, relheight=h)
v1_mean = Entry(root)
v1_mean.place(relx=posx1, rely=posy+4*offset_y, relwidth=entryw, relheight=h)
v1_mean.insert(0,"0.0")


# Inicializa gráfico
iniciar_grafico()
root.mainloop()
