from tkinter import *
from tkinter import ttk, simpledialog
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import threading
from PIL import Image, ImageTk
import pyvisa
import time
import os
import csv
import re
import traceback

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

# caminho da pasta de resultados (garantido depois)
path_pasta = None

# ========================== inicialização ==========================

session_name = None

def pedir_nome_sessao():
    global session_name
    temp_root = Tk()
    temp_root.withdraw()  # Esconde janela
    session_name = simpledialog.askstring("Nome da Sessão", "Digite o nome da sessão:", parent=temp_root)
    if not session_name:
        # adiciona timestamp ao default para evitar sobrescrita
        session_name = f"default_session_{time.strftime('%Y%m%d_%H%M%S')}"
    temp_root.destroy()

def checar_criar_pasta():
    global path_pasta
    base = r".\6517b_files"
    if not os.path.exists(base):
        os.makedirs(base)
        print(f"Pasta base '{base}' criada.")
    
    # cria subpasta com o nome da sessão
    path_pasta = os.path.join(base, session_name)
    if not os.path.exists(path_pasta):
        os.makedirs(path_pasta)
        print(f"Pasta de sessão '{path_pasta}' criada.")
    else:
        print(f"Pasta de sessão '{path_pasta}' já existe.")



# pede o nome da sessão antes de abrir a janela principal
pedir_nome_sessao()
#cria a pasta com os arquivos destino
checar_criar_pasta()
# Cria log já com o nome da sessão dentro da pasta
def criar_log():
    nome = session_name if session_name else "default_log"
    log_file = os.path.join(path_pasta, f"{nome}.log")
    try:
        with open(log_file, "w", encoding="utf-8") as f:
            f.write(f"Log iniciado em {time.ctime()}\n")
    except Exception:
        print("Erro criando log inicial:\n", traceback.format_exc())
    return log_file

log_file = criar_log()

def escrever_log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    try:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"[{timestamp}] {msg}\n")
    except Exception:
        print("Erro escrevendo no log:\n", traceback.format_exc())
    print(f"[{timestamp}] {msg}")  # também no console

# ===================== JANELA PRINCIPAL =====================
root = Tk()

# ============ conexão / comandos ao multímetro ============
def conectar():
    global multimeter
    try:
        rm = pyvisa.ResourceManager()
        multimeter = rm.open_resource('GPIB0::27::INSTR')
        escrever_log('Conexão iniciada com GPIB0::27::INSTR')
    except Exception as e:
        multimeter = None
        try:
            status_label.config(text=f"Erro: {e}", fg="red")
        except Exception:
            pass
        escrever_log(f"Não conectou com o keithley: {e}")
        return

    # configurações básicas (se conectado)
    try:
        multimeter.write(":SYSTem:ZCHeck ON")
        escrever_log("Comando ':SYSTem:ZCHeck ON'")
        multimeter.write(":SENSe:FUNCtion 'CURRent'")
        escrever_log("Comando \":SENSe:FUNCtion 'CURRent'\"")
        multimeter.write(":SENSe:CURRent:RANGe:AUTO OFF")
        escrever_log("Comando ':SENSe:CURRent:RANGe:AUTO OFF'")
        status_label.config(text="Conectado Keithley 6517B", fg="green")
    except Exception as e:
        escrever_log(f"Erro ao configurar instrumento: {e}")

def safe_query(cmd):
    """Faz query ao instrumento de forma segura; retorna string ou None."""
    if not multimeter:
        escrever_log("safe_query: multimeter não conectado.")
        return None
    try:
        resp = multimeter.query(cmd)
        return resp
    except Exception as e:
        escrever_log(f"Erro na query '{cmd}': {e}")
        return None
"""
def meas_current():
    
    Tenta obter a corrente do instrumento.
    Faz parsing tolerante de números na resposta.
    Retorna float (A). Se falhar, retorna 0.0 e escreve no log.
    
    # tenta diferentes comandos comuns; o seu driver pode aceitar outros
    candidates = [":MEASure:CURRent:DC?", ":READ?", ":SENSe:DATA:FRESh?"]
    resp = None
    for cmd in candidates:
        resp = safe_query(cmd)
        if resp:
            break
    if not resp:
        escrever_log("meas_current: sem resposta do instrumento, retornando 0.0 A")
        return 0.0
    # extrai primeiro número com regex (aceita notação científica)
    m = re.search(r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?", resp)
    if m:
        try:
            val = float(m.group(0))
            return val
        except Exception:
            escrever_log(f"Erro convertendo '{m.group(0)}' para float")
            return 0.0
    else:
        escrever_log(f"meas_current: não consegui parsear resposta: '{resp}'")
        return 0.0
"""
def meas_current():
    try:
        return float(multimeter.query(':SENSe:DATA:FRESh?').split(',')[0][:-4])
    except Exception as e:
        escrever_log(f"Erro em meas_current:{e}")

def out_v(v):
    if not multimeter:
        escrever_log("out_v: multimeter não conectado. Ignorando comando.")
        return
    try:
        multimeter.write(f":SOURce:VOltage:LEVel:IMMediate:AMPLitude {v}")
    except Exception as e:
        escrever_log(f"Erro em out_v({v}): {e}")

# ============ inicialização do gráfico ============
def iniciar_grafico():
    global fig, ax_scan, ax_monitor, canvas
    fig, (ax_scan, ax_monitor) = plt.subplots(2,1, figsize=(6,6))
    fig.subplots_adjust(left=0.35, right=0.95, top=0.93, bottom=0.1, hspace=0.35)
    fig.tight_layout(pad=3.0)

    # Scan
    ax_scan.set_title("Scan", fontsize=14)
    ax_scan.set_xlabel("Applied Voltage (V)", fontsize=12)
    ax_scan.set_ylabel("Current (nA)", fontsize=12)

    # Monitor
    ax_monitor.set_title("Monitor", fontsize=14)
    ax_monitor.set_xlabel("Time (s)", fontsize=12)
    ax_monitor.set_ylabel("Current (nA)", fontsize=12)

    canvas = FigureCanvasTkAgg(fig, master=root)
    canvas.draw()
    canvas.get_tk_widget().place(relx=0.4, rely=0.0, relwidth=0.55, relheight=1.0)

    fundo()

def fundo():
    global bg_photo
    try:
        # caminho local - ajuste para seu PC
        bg_image = Image.open(r"C:\Users\Hepic\Desktop\6517B\Daniel\logo_simples.jpg")
        # para deixar mais portátil, tentamos carregar imagem relativa à pasta atual
        #possible = [
        #    os.path.join(os.getcwd(), "logo_simples.jpg"),
        #    os.path.join(os.getcwd(), "imagens", "logo_simples.jpg")
        #]
        #bg_image = None
        #for p in possible:
        #    if os.path.exists(p):
        #        bg_image = Image.open(p)
        #        break
        #if bg_image is None:
        #    return
        ratio = bg_image.height / bg_image.width
        bg_image = bg_image.resize((300, int(ratio*300)))
        bg_photo = ImageTk.PhotoImage(bg_image)
        bg_label = Label(root, image=bg_photo, borderwidth=0)
        bg_label.place(relx=0.02, rely=0.97, anchor="sw")
    except Exception as e:
        print(f"Erro carregando fundo: {e}")

# ================ desenho dos gráficos ================
def desenha_scan(resultados):
    try:
        if not resultados: 
            canvas.draw()
            return
        ax_scan.clear()
        ax_scan.set_xlabel("Applied Voltage (V)")
        ax_scan.set_ylabel("Current (nA)")
        data = np.array(resultados)
        ax_scan.plot(data[:,0], data[:,1]*1e9, marker='o', linestyle='-', markerfacecolor='red')
        ax_scan.set_title("Scan")
        canvas.draw()
    except Exception:
        escrever_log("Erro em desenha_scan:\n" + traceback.format_exc())

def desenha_monitor(resultados):
    try:
        if not resultados:
            canvas.draw()
            return
        ax_monitor.clear()
        ax_monitor.set_xlabel("Time (s)")
        ax_monitor.set_ylabel("Current (nA)")
        data = np.array(resultados)
        ax_monitor.plot(data[:,0], data[:,1]*1e9, marker='o', linestyle='-', markerfacecolor='red')
        ax_monitor.set_title("Monitor")
        canvas.draw()
    except Exception:
        escrever_log("Erro em desenha_monitor:\n" + traceback.format_exc())

# =============== SCAN com monitor e gravação em tempo real ============
def scan_com_monitor(stop_event):
    """
    Realiza varredura em V; enquanto aguarda estabilização, grava pontos de monitor
    em tempo real e atualiza gráficos. Quando estabiliza (dI < limite_delta),
    salva ponto no arquivo de scan e segue para próxima tensão.
    """
    global resultados_scan, resultados_monitor
    escrever_log("Função Scan+Monitor iniciada")

    if not multimeter:
        escrever_log("Instrumento não conectado: operação abortada.")
        return

    # configura corrente e saída
    valor_range = vl_res.get(combo.get(), "20E-3")  # pega do dicionário
    try:
        multimeter.write(":SENSe:CURRent:RANGe:AUTO OFF")
        multimeter.write(f":SENSe:CURRent:DC:RANGe {valor_range}")
        escrever_log(f"Comando ':SENSe:CURRent:DC:RANGe {valor_range}'")
        multimeter.write(":SYSTem:ZCHeck OFF")
        escrever_log(f"Comando ':SYSTem:ZCHeck OFF'")
        multimeter.write(":OUTput1:STATe ON")
        escrever_log("Comando ':OUTput1:STATe ON'")
        multimeter.write(":SOURce:VOLTage:MCONnect ON")
        escrever_log("Comando ':SOURce:VOLTage:MCONnect ON'")
        multimeter.write(":SOURce:VOLTage:RANGe MAXimum")
        escrever_log("Comando ':SOURce:VOLTage:RANGe MAXimum'")
    except Exception as e:
        escrever_log(f"Erro ao enviar comandos iniciais: {e}")

    resultados_scan = []
    resultados_monitor = []

    # arquivos para escrita em tempo real
    scan_file = os.path.join(path_pasta, f"scan_{session_name}.csv")
    monitor_file = os.path.join(path_pasta, f"monitor_{session_name}.csv")

    try:
        f_scan = open(scan_file, "w", newline="", encoding="utf-8")
        writer_scan = csv.writer(f_scan)
        writer_scan.writerow(["V (V)", "I (A)"])

        f_monitor = open(monitor_file, "w", newline="", encoding="utf-8")
        writer_monitor = csv.writer(f_monitor)
        writer_monitor.writerow(["Tempo (s)", "I (A)"])
    except Exception as e:
        escrever_log(f"Erro abrindo arquivos de saída: {e}")
        return

    try:
        v_ini = 0.0
        try:
            v_fim = float(v1.get())
        except Exception:
            escrever_log("Valor alvo V inválido; abortando scan.")
            v_fim = 0.0
        try:
            dv = float(passo.get())
        except Exception:
            escrever_log("Passo inválido; usando 0.1 V")
            dv = 0.1

        if dv == 0:
            escrever_log("Passo=0 inválido; ajustando para 0.1")
            dv = 0.1

        x_values = list(np.arange(v_ini, v_fim + dv, dv))

        limite_delta = 1e-10  # limite de corrente considerada como estável
        timeout = 60           # tempo máximo por passo (s)

        t0_global = time.time()

        for v in x_values:
            if stop_event.is_set():
                escrever_log("Scan interrompido pelo usuário.")
                break

            out_v(v)
            escrever_log(f"Scan Aplicado V={v} V")

            t0 = time.time()
            

            lista_corrente = []
            n_media = 15

            while True:
                if stop_event.is_set():
                    escrever_log("Recebido stop dentro do passo.")
                    break
                
                t_rel = time.time() - t0_global
                i = meas_current()

                # monitoramento em tempo real
                resultados_monitor.append((t_rel, i))
                writer_monitor.writerow([f"{t_rel:.6f}", f"{i:.12e}"])
                f_monitor.flush()
                desenha_monitor(resultados_monitor)

                # adiciona ao histórico
                lista_corrente.append(i)
                if len(lista_corrente) > n_media:
                    lista_corrente.pop(0)

                # critério mais rigoroso: delta nas últimas n leituras
                if len(lista_corrente) == n_media:
                    delta_media = max(lista_corrente) - min(lista_corrente)
                    if delta_media < limite_delta:
                        escrever_log(f"Estabilizou em t={t_rel:.2f} s, I={i:.3e} A")
                        break
                    
                if time.time() - t0 > timeout:
                    escrever_log(f"Timeout em t={t_rel:.2f} s, I={i:.3e} A")
                    break

            # salva ponto estabilizado no scan (em memória e arquivo)
            resultados_scan.append((v, i))
            try:
                writer_scan.writerow([f"{v:.6f}", f"{i:.12e}"])
                f_scan.flush()
            except Exception:
                escrever_log("Erro escrevendo linha no scan_file:\n" + traceback.format_exc())

            try:
                desenha_scan(resultados_scan)
            except Exception:
                pass

        escrever_log("Scan finalizado. Iniciando rampdown automático.")
        if len(resultados_scan) > 0:
            v_final = resultados_scan[-1][0]
            rampdown(stop_event, v_final, passo=-10, delay=1)  # ajustável
        else:
            escrever_log("Scan não gerou resultados, pulando rampdown.")

        escrever_log(f"Resultados salvos em '{scan_file}' e '{monitor_file}' (em tempo real).")
    except Exception:
        escrever_log("Erro durante o scan:\n" + traceback.format_exc())
    finally:
        try:
            f_scan.close()
            f_monitor.close()
        except Exception:
            pass
    
    multimeter.write(":SYSTem:ZCHeck ON")
    escrever_log("Comando: :SYSTem:ZCHeck ON")
    return resultados_scan, resultados_monitor

# ===================== rampdown =====================
def rampdown(stop_event, v_atual, passo=1.0, delay=0.2):
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

    out_v(0)
    try:
        if multimeter:
            multimeter.write(":OUTput1:STATe OFF")
    except Exception as e:
        escrever_log(f"Erro desligando saída: {e}")
    escrever_log("Rampdown finalizado. Saída desligada.")
    try:
        status_label.config(text="Tensão zerada", fg="blue")
    except Exception:
        pass

# =========== média no monitoramento ===========
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
    ax_monitor.axhline(media_corrente*1e9, linestyle='--', label=f"Média ({t0:.2f}s-{t1:.2f}s) = {media_corrente*1e9:.2f} nA")
    ax_monitor.legend()
    canvas.draw()
    escrever_log("Gráfico de média plotado.")

# ================ start/stop ================
def iniciar_medicao():
    escrever_log("Tomada de dados iniciada")
    global stop_event
    stop_event = threading.Event()
    t_scan = threading.Thread(target=scan_com_monitor, args=(stop_event,), daemon=True)
    t_scan.start()

def stop():
    global stop_event
    if stop_event:
        stop_event.set()
        escrever_log('Botão de stop pressionado')

# =============== salvar manualmente ================
def salvar_como():
    global resultados_scan, resultados_monitor
    nome = session_name

    if not resultados_scan and not resultados_monitor:
        escrever_log("Nenhum dado para salvar!")
        return

    # Scan
    if resultados_scan:
        try:
            data_scan = np.array(resultados_scan)
            caminho_scan = os.path.join(path_pasta, f"{nome}_scan.csv")
            np.savetxt(caminho_scan, data_scan, delimiter=",", header="Tensão (V), Corrente (A)", comments="", fmt="%f,%e")
            escrever_log(f"Scan salvo em {caminho_scan}")
        except Exception as e:
            escrever_log(f"Erro salvando scan manualmente: {e}")

    # Monitor
    if resultados_monitor:
        try:
            data_monitor = np.array(resultados_monitor)
            caminho_monitor = os.path.join(path_pasta, f"{nome}_monitor.csv")
            np.savetxt(caminho_monitor, data_monitor, delimiter=",", header="Tempo (s), Corrente (A)", comments="", fmt="%f,%e")
            escrever_log(f"Monitor salvo em {caminho_monitor}")
        except Exception as e:
            escrever_log(f"Erro salvando monitor manualmente: {e}")

# =============== salvar figura ===============
def save_graph():
    global fig
    nome = session_name
    if not nome:
        escrever_log("Insira um nome de arquivo válido!")
        return
    caminho_fig = os.path.join(path_pasta, f"{nome}.png")
    try:
        fig.savefig(caminho_fig, dpi=300)
        escrever_log(f"Gráfico salvo em {caminho_fig}")
    except Exception as e:
        escrever_log(f"Erro salvando figura: {e}")

# =============== limpar ===============
def limpar():
    global resultados_scan, resultados_monitor
    resultados_scan = []
    resultados_monitor = []
    try:
        ax_scan.clear()
        ax_scan.set_xlabel("Applied Voltage (V)")
        ax_scan.set_ylabel("Current (nA)")
        ax_scan.set_title("Scan")

        ax_monitor.clear()
        ax_monitor.set_xlabel("Time (s)")
        ax_monitor.set_ylabel("Current (nA)")
        ax_monitor.set_title("Monitor")

        canvas.draw()
    except Exception:
        pass
    escrever_log('Botão "Clear" pressionado')

# ======= fechar a janela e conexão ========
def on_exit():
    try:
        if multimeter:
            multimeter.close()
    except Exception:
        pass
    escrever_log("Sessão finalizada.")
    root.destroy()

# ===================== WIDGETS ======================
root.title("6517B Interface")
root.configure(background=cor10)
root.geometry("1000x600")
root.protocol("WM_DELETE_WINDOW", on_exit)
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

graph_btn = Button(root, text="Save Graph", relief="groove", command=save_graph)
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
