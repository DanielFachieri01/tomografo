import pyvisa
import time
import matplotlib.pyplot as plt
import matplotlib.style as mplstyle
def main():
    keep=1
    loop=open("loopgreg.txt","r")
    linha=loop.readline()
    repet=int(linha)
    log=open("log_raio-x%d.txt"%(repet),"w")
    log.write("Aqui começamos")
    print("Aqui começamos")
    dados=open("dados_raio-x%d.csv"%(repet),"w")
    repet+=1
    loop.close()
    loop1=open("loopgreg.txt","w+")
    loop1.write("%d"%(repet))
    loop1.flush()
    dados.write("Tempo(s)   Corrente(nA) \n")
    # Initialize the keithley and create some useful variables
    multimeter = pyvisa.ResourceManager().open_resource('GPIB0::27::INSTR')# Connect to the keithley and set it to a variable named multimeter.
    multimeter.write(":SENSe:FUNCtion 'CURRent'") # Set the keithley to measure temperature.
    log.write(":SENSe:FUNCtion 'CURRent \n")
    multimeter.write(":SENSe:CURRent:DC:RANGe:AUTO OFF")
    multimeter.write(":SYSTem:ZCHeck OFF")
    log.write(":SYSTem:ZCHeck OFF \n")
    log.write(":SENSe:CURRent:DC:RANGe:AUTO OFF \n")
    timeList = [] # Create an empty list to store time values in.
    currentList = [] # Create an empty list to store temperature values in.
    starttime= time.time()
    # Setup the plot 
    plt.figure(figsize=(10,10)) # Initialize a matplotlib figure
    plt.xlabel('time (s)', fontsize=24) # Create a label for the x axis and set the font size to 24pt
    plt.xticks(fontsize=18) # Set the font size of the x tick numbers to 18pt
    plt.ylabel('Current (nA)', fontsize=24) # Create a label for the y axis and set the font size to 24pt
    plt.yticks(fontsize=18) # Set the font size of the y tick numbers to 18pt
    mplstyle.use ('fast')
    aux_time_cont=1 #tentativa número um de arrumar o tempo dos dados (usar esse por enquanto) os dados não condizem com o gráfico
    #exp_time=float(time.time()-starttime)#tentativa número dois de arruma o tempo dos dados(não deu certo!!)
    while keep==1:
        currentReading = float(multimeter.query(':SENSe:DATA:FRESh?').split(',')[0][:-4])*10**9 # Read and process data from the keithley. Reading in mA
        log.write(':SENSe:DATA:FRESh? \n')
        dados.write("%lf, %lf \n"%(aux_time_cont,currentReading))
        dados.flush()
        log.flush()
        currentList.append(currentReading) # Append processed data to the temperature list
        timeList.append(float(time.time()-starttime)) # Append time values to the time list
        time.sleep(0.1) # Interval to wait between collecting data points.
        plt.plot(timeList, currentList, color='black', linewidth=2) # Plot the collected data with time on the x axis and temperature on the y axis.
        plt.scatter(timeList, currentList, color='red', s=25)
        plt.pause(0.05) # This command is required for live plotting. This allows the code to keep running while the plot is shown.
        aux_time_cont = aux_time_cont + 1
    multimeter.write(":SYSTem:ZCHeck ON")
    log.write(":SYSTem:ZCHeck OFF \n")
    log.write("======================================")
    dados.flush()
    log.flush() 
    plt.show()
    print("ACABOU?")
    quit()
    
main()
