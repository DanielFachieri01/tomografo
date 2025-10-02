from tkinter import *
from tkinter import ttk, filedialog,simpledialog
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import threading
from PIL import Image, ImageTk  
import pyvisa
import time
import os

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
stop_event = None
resultados_scan = []
resultados_monitor = []

# ========================== inicialização ==========================

#criar pasta para armazenar as informações
def checar_criar_pasta():
    global path_pasta
    path_pasta = "6517b_files"
    if not os.path.exists(path_pasta):
        os.makedirs(path_pasta)
        print(f"Pasta '{path_pasta}' criada.")
    else:
        print(f"Pasta '{path_pasta}' já existe.")

session_name = None

def pedir_nome_sessao():
    global session_name
    temp_root = Tk()
    temp_root.withdraw()  # Esconde janela principal

    session_name = simpledialog.askstring("Nome da Sessão", "Digite o nome da sessão:", parent=temp_root)
    if not session_name:
        session_name = "default_session"

    temp_root.destroy()

# pede o nome da sessão antes de abrir a janela principal
pedir_nome_sessao()

# Cria log já com o nome da sessão dentro da pasta
def criar_log():
    nome = session_name
    if not nome:
        nome = "default_log"
    log_file = os.path.join(path_pasta, f"{nome}.log")
    with open(log_file, "w") as f:
        f.write(f"Log iniciado em {time.ctime()}\n")
    return log_file

log_file = criar_log()

def escrever_log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with open(log_file, "a") as f:
        f.write(f"[{timestamp}] {msg}\n")
    print(msg)  # também no console


# só agora abre a janela principal
root = Tk()

def conectar():
    global multimeter
    try:
        rm = pyvisa.ResourceManager()
        multimeter = rm.open_resource('GPIB0::27::INSTR')
        escrever_log(f'conexão iniciada com "open_resource("GPIB0::27::INSTR")" ')


    except Exception as e:
        multimeter = None
        status_label.config(text=f"Erro: {e}", fg="red")
        escrever_log(f"Não conectou com o keithley ")
    
    if multimeter is not None:
        multimeter.write(":SYSTem:ZCHeck ON")
        escrever_log(":SYSTem:ZCHeck ON")

        multimeter.write(":SENSe:FUNCtion 'CURRent'")
        escrever_log("Comando ':SENSe:FUNCtion 'CURRent'")
  
        multimeter.write(":SENSe:CURRent:RANGe:AUTO OFF")
        escrever_log("Comando ':SENSe:CURRent:RANGe:AUTO OFF'")

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
    escrever_log("função Scan iniciada")

    # Configura corrente e saída
    multimeter.write(":SENSe:CURRent:DC:RANGe 1e-7")  # ajustado para < 40 nA
    escrever_log("Comando ':SENSe:CURRent:DC:RANGe 1e-7'")
    status_label.config(text="Conectado Keithley 6517B", fg="green")
    multimeter.write(":OUTput1:STATe ON")
    escrever_log("Comando ':OUTput1:STATe ON'")
    multimeter.write(":SOURce:VOLTage:MCONnect ON")
    escrever_log("Comando ':SOURce:VOLTage:MCONnect ON'")
    multimeter.write(":SOURce:VOLTage:RANGe MAXimum")
    escrever_log("Comando ':SOURce:VOLTage:RANGe MAXimum'")

    resultados_scan = []
    v_ini = 0.0
    v_fim = float(v1.get())
    dv = float(passo.get())
    x_values = list(np.arange(v_ini, v_fim+dv, dv))

    limite_delta = 100e-9   # limite de corrente que é considerada como estável
    timeout = 10          # tempo máximo por passo (s)

    for v in x_values:
        if stop_event.is_set():
            escrever_log("Scan interrompido pelo usuário.")
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
                escrever_log(f"Estabilizou em V={v} V, I={i:.3e} A")
                break
            if time.time() - t0 > timeout:
                escrever_log(f"Timeout em V={v} V, I={i:.3e} A")
                break
            i_past = i

        resultados_scan.append((v, i))
        desenha_scan(resultados_scan)
        # Final do scan

    escrever_log("Scan finalizado. Iniciando rampdown automático.")
    if len(resultados_scan) > 0:
        v_final = resultados_scan[-1][0]
        rampdown(stop_event, v_final, passo=10, delay=1)  # ajustável
    else:
        escrever_log("Scan não gerou resultados, pulando rampdown.")

    return resultados_scan


# ===================== monitoramento =====================
def monitorar(stop_event):
    """
    Aqui é um monitoramento que assegura o funcionamento da fonte de alta tensão 
    """
    global resultados_monitor
    escrever_log("Monitor iniciando")
    resultados_monitor = []
    t0 = time.time()
    i_past = meas_current()
    dI_lim = 20e-6  # limite de variação máxima

    while not stop_event.is_set():
        time.sleep(0.1)
        i = meas_current()
        t1 = time.time()
        resultados_monitor.append((t1 - t0, i))
        desenha_monitor(resultados_monitor)

        dI = abs(i - i_past)
        if dI > dI_lim:
            stop_event.set()
            escrever_log(f"Monitor parou: dI máximo atingido = {dI:.3e} A")
            break
        i_past = i

    return resultados_monitor

# ===================== rampdown =====================
def rampdown(stop_event, v_atual, passo=1.0, delay=0.2):
    """
    Reduz a tensão aplicada até 0 V em por passos
    v_atual: tensão final do scan
    passo: passo de descida (V)
    delay: tempo entre passos (s)
    """
    escrever_log(f"Iniciando rampdown a partir de {v_atual} V")

    v = v_atual
    while v > 0:
        if stop_event.is_set():
            escrever_log("Rampdown interrompido pelo usuário.")
            break

        v = max(0, v - passo)
        out_v(v)
        escrever_log(f"Rampdown: V={v} V")
        time.sleep(delay)

    # Garante que zerou
    out_v(0)
    multimeter.write(":OUTput1:STATe OFF")
    escrever_log("Rampdown finalizado. Saída desligada.")
    status_label.config(text="Tensão zerada", fg="blue")



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
    escrever_log(f"gráfico de média plotado ")


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
    escrever_log(f"Tomada de dados iniciada ")
    global stop_event
    stop_event = threading.Event()
    t_scan = threading.Thread(target=scan, args=(stop_event,))
    t_monitor = threading.Thread(target=monitorar, args=(stop_event,))
    t_scan.start()
    t_monitor.start()

def stop():
    if stop_event:
        stop_event.set()
        escrever_log(f'botão de stop pressionado ')

# ===================== salvar dados =====================

def salvar_como():
    global resultados_scan, resultados_monitor, log
    nome = session_name

    if not resultados_scan and not resultados_monitor:
        print("Nenhum dado para salvar!")
        escrever_log("Nenhum dado para salvar!")
        return

    # Scan
    if resultados_scan:
        data_scan = np.array(resultados_scan)
        caminho_scan = os.path.join(path_pasta, f"{nome}_scan.csv")
        np.savetxt(caminho_scan, data_scan, delimiter=",", header="Tensão (V), Corrente (A)", comments="")
        print(f"Scan salvo em {caminho_scan}")
        escrever_log(f"Scan salvo em {caminho_scan}")

    # Monitor
    if resultados_monitor:
        data_monitor = np.array(resultados_monitor)
        caminho_monitor = os.path.join(path_pasta, f"{nome}_monitor.csv")
        np.savetxt(caminho_monitor, data_monitor, delimiter=",", header="Tempo (s), Corrente (A)", comments="")
        print(f"Monitor salvo em {caminho_monitor}")
        escrever_log(f"Monitor salvo em {caminho_monitor}")
    
# ===================== salvar gráfico =====================
def save_graph(fig):
    nome = session_name
    if not nome:
        print("Insira um nome de arquivo válido!")
        return
    caminho_fig = os.path.join(path_pasta, f"{nome}.png")
    fig.savefig(caminho_fig, dpi=300)
    print(f"Gráfico salvo em {caminho_fig}")
    escrever_log(f"Gráfico salvo em {caminho_fig}")
    
# ===================== clear =====================

def limpar():
    global resultados_scan, resultados_monitor
    resultados_scan = []
    resultados_monitor = []
    ax_scan.clear()
    ax_scan.set_xlabel("Tensão (V)")
    ax_scan.set_ylabel("Corrente (nA)")
    ax_scan.set_title("Caracterização")
    
    ax_monitor.clear()
    ax_monitor.set_xlabel("Tempo (s)")
    ax_monitor.set_ylabel("Corrente (nA)")
    ax_monitor.set_title("Monitoramento")
    
    canvas.draw()
    escrever_log('Botão de "clear" pressionado')

# ======= fechar a janela e quebrar a conexão ========
def on_exit():
    if multimeter:
        try: multimeter.close()
        except: pass
    escrever_log("Sessão finalizada.")
    root.destroy()

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

# Seleção da resolução

opt = ["20 mA","2mA","200 uA", "20 uA","2 uA","200 nA","20 nA","2 nA","200 pA","20 pA"]
vl_res = {opt[0] : "20E-3", opt[1] : "2E-3",opt[2] : "200E-6",opt[3] :"20E-6",opt[4] :"2E-6",opt[5] :"200E-9",opt[6] :"20E-9",opt[7] :"2E-9",opt[8]:"200E-12",opt[9]:"20E-12"}

resolucao = StringVar()       
combo = ttk.Combobox(root, textvariable=resolucao, values=opt, state="readonly")
combo.set(opt[0])
combo.place(relx=posx1, rely=posy+5*offset_y, relwidth=entryw+0.03, relheight=h)


# Inicializa gráfico
iniciar_grafico()
root.mainloop()
