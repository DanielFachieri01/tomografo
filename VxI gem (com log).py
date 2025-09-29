import pyvisa
import time
import matplotlib.pyplot as plt

num_medidas=1
volt_start=200
delta_v=10
volt_max=1000
rampa_volt_passos=5

with open("log_VxI.txt","w") as log:
    log.write("Aqui começamos")
    print("Aqui começamos")
    dados=open("dados_VxI.txt","w")
    dados.write("Tensão (V)     Corrente (nA) \n")
    # Initialize the keithley and create some useful variables
    multimeter = pyvisa.ResourceManager().open_resource('GPIB0::27::INSTR')# Connect to the keithley and set it to a variable named multimeter.
    multimeter.write(":SENSe:FUNCtion 'CURRent'") # Set the keithley to measure temperature.
    log.write(":SENSe:FUNCtion 'CURRent \n")
    multimeter.write(":SENSe:CURRent:DC:RANGe:AUTO OFF")
    multimeter.write(":SYSTem:ZCHeck OFF")
    log.write(":SYSTem:ZCHeck OFF \n")
    log.write(":SENSe:CURRent:DC:RANGe:AUTO OFF \n")
    multimeter.write(":OUTput1:STATe ON")
    log.write(":OUTput1:STATe ON \n")
    multimeter.write(":SOURce:VOLTage:MCONnect ON ")
    log.write(":SOURce:VOLTage:MCONnect ON \n")
    multimeter.write(":SOURce:VOLTage:RANGe MAXimum")
    time.sleep(1)
    log.write (":SOURce:VOLTage:RANGe MAXimum \n")
    timeList = [] # Create an empty list to store time values in.
    temperatureList = [] # Create an empty list to store temperature values in.

    # Setup the plot 
    plt.figure(figsize=(10,10)) # Initialize a matplotlib figure
    plt.xlabel('Voltage (V)', fontsize=24) # Create a label for the x axis and set the font size to 24pt
    plt.xticks(fontsize=18) # Set the font size of the x tick numbers to 18pt
    plt.ylabel('Current (nA)', fontsize=24) # Create a label for the y axis and set the font size to 24pt
    plt.yticks(fontsize=18) # Set the font size of the y tick numbers to 18pt

    volt=volt_start
    # Create a while loop that continuously measures and plots data from the keithley forever.
    while volt<=volt_max:
        multimeter.write(":SOURce:VOltage:LEVel:IMMediate:AMPLitude %d"%(volt))
        log.write(":SOURce:VOltage:LEVel:IMMediate:AMPLitude %d \n"%(volt))
        k=0
        time.sleep(15)
        print("começando a tomar medidas em volt=%d"%(volt))
        while k<num_medidas:

            temperatureReading = float(multimeter.query(':SENSe:DATA:FRESh?').split(',')[0][:-4])*10**9 # Read and process data from the keithley.
            log.write(':SENSe:DATA:FRESh? \n')
            dados.write("%d %f \n"%(volt,temperatureReading))
            dados.flush()
            log.flush()
            temperatureList.append(temperatureReading) # Append processed data to the temperature list
            timeList.append(volt) # Append time values to the time list
            time.sleep(3) # Interval to wait between collecting data points.
            plt.plot(timeList, temperatureList, color='black', linewidth=2) # Plot the collected data with time on the x axis and temperature on the y axis.
            plt.scatter(timeList, temperatureList, color='red', s=25)
            plt.pause(0.01) # This command is required for live plotting. This allows the code to keep running while the plot is shown.
            k+=1
        for i in range(rampa_volt_passos):
            volt+=delta_v
            multimeter.write(":SOURce:VOltage:LEVel:IMMediate:AMPLitude %d"%(volt))
            time.sleep(0.1)
            log.write(":SOURce:VOltage:LEVel:IMMediate:AMPLitude %d \n"%(volt))
            time.sleep(0.1)
            time.sleep(1)
            


    multimeter.write(":SOURce:VOltage:LEVel:IMMediate:AMPLitude 0")
    log.write(":SOURce:VOltage:LEVel:IMMediate:AMPLitude 0 \n")
    multimeter.write(":OUTput1:STATe OFF")
    multimeter.write(":SYSTem:ZCHeck ON")
    log.write(":SYSTem:ZCHeck OFF \n")
    log.write(":OUTput1:STATe OFF \n")
    log.write("======================================")
    dados.flush()
    log.flush()    

    plt.show()
    print("ACABOU?")
    quit()
    