# uses the curses library to make a terminal screen that allows
# the user to communicate with Atlas Scientific boards

import curses, curses.ascii # needed to allow seperate terminal displays
import serial # required for communication with boards
from time import strftime # used for timestamps

def main(stdscr):
    #screen parameters
    pos_text = 0 # the position of the cursor in the user input area
    stdscr.nodelay(1) # stops the terminal from waiting for user input
    height,width = stdscr.getmaxyx() # gets the height and width of the terminal window
    max_pad_length = 100 # the maximum length of the received text buffer
    pad = curses.newpad(max_pad_length, width) # creates a text area buffer that holds max_pad_length lines
    # what happens when it runs out? should we handle that?
    inputpad = stdscr.subpad(1, width, height-1, 0) # creates the area to input text on the bottom
    
    #USB parameters
    usbport = '/dev/ttyAMA0'
    ser = serial.Serial(usbport, 9600, timeout = 0) # sets the serial port to the specified port, with a 9600 baud rate
    # Timeout = 0 tells the serial port to not wait for input if there is non 
    
    # declare and initialize the data buffers
    line = "" 
    user_input = ""

    #clear serial buffer
    ser.write("\r")

    #turn on LEDS
    ser.write("L,1\r")

    #enable streaming
    ser.write("C,1\r")
    
    #temp compensation
    ser.write("T,15.75\r")
    
    def check_ph_level(line):
        # compares the ph reading to a value and displays if its higher or lower
        try:
            ph = float(line) # converts the string into a floating point number
            if(ph >= 7.7):
                pad.addstr("High"+ "\n")
            if(ph < 7.4):
                pad.addstr("Low"+ "\n")
            else:
            	pad.addstr("Normal"+ "\n")
        except ValueError:
            # if the string is not a valid floating point number, dont do anything
            pass
    
    #main loop
    while True:
    
        inputpad.refresh() # refresh the input areaevery loop
        y,x = pad.getyx() # gets the position of the cursor in the main screen
        if(y >= max_pad_length-3): # clear screen when at the end of buffer
            pad.erase()
            pad.move(0,0)
        
        
        # sensor receive
        data = ser.read() # get serial data
        if(data == "\r"): # if its terminated by a newline
            pad.addstr("> " + strftime("%Y-%m-%d %H:%M:%S") + 
            " Received from sensor: " + line + "\n") #print the timestamp and received data to the main screen
            check_ph_level(line) # calls function to check ph levels
            line = "" # clears the input
            pad.refresh(y-(height-3),0, 0,0, height-2,width) # refresh the main screen and scroll to the cursor
            
        else:
            line  = line + data # if the line isn't complete, add the new characters to it
        
        # user receive
        c= stdscr.getch()
        if c != -1:
            if(c == curses.KEY_BACKSPACE):
                # if the backspace character is pressed, clear the current user input
                pos_text = 0
                inputpad.clear()
                user_input = ""
                
            elif(c == ord('\n') or c == ord('\r')):
                # if the enter key is pressed, send the input to the board and clear the user input
                ser.write(user_input + '\r')
                pad.addstr("Sent to board: " + user_input+ "\n")
                pos_text = 0
                inputpad.clear()
                user_input = ""
                
            elif(curses.ascii.isprint(c)):
                # if the character is a printable character, put it into the input area and send buffer
    	        user_input = user_input + chr(c)
    	        inputpad.addstr(0, pos_text, chr(c))
                inputpad.move(0, pos_text)
                pos_text = (pos_text + 1) % (width -1) # increments the cursors position in the input area, returns it to the beginning if it runs past the screen size
            else:
                #if the character entered doesnt meet any conditions, ignore it
                pass
            
if __name__ == '__main__':
    curses.wrapper(main) # wraps the curses window to undo the changes it makes to the terminal on exits and exceptions

