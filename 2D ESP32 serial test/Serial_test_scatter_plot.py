#Serial_test.py

import time
import serial


from datetime import datetime
from pprint import pprint
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np

class IR_camera_object(object):

    def __init__(self):
        self.x_leftCam = []
        self.y_leftCam = []
        self.x_rightCam = []
        self.y_rightCam = []
        self.leftCamFeatures = []
        self.rightCamFeatures = []

    def set_left_cam_features(self,leftCam_features):
        self.leftCamFeatures.append(leftCam_features)
        self.x_leftCam.append(leftCam_features['center_x'])
        self.y_leftCam.append(leftCam_features['center_y'])

    def set_right_cam_features(self,rightCam_features):
        self.rightCamFeatures.append(rightCam_features)
        self.x_rightCam.append(rightCam_features['center_x'])
        self.y_rightCam.append(rightCam_features['center_y'])
        
    def get_left_cam_x_coordinates(self):
        return self.x_leftCam 
    
    def get_left_cam_y_coordinates(self):
        return self.y_leftCam 
 
    def get_right_cam_x_coordinates(self):
        return self.x_rightCam 
    
    def get_right_cam_y_coordinates(self):
        return self.y_rightCam 
    
    def get_leftCam_obj_coordinates(self):
        return np.c_[np.array(self.x_leftCam),np.array(self.y_leftCam)]

    def get_rightCam_obj_coordinates(self):
        return np.c_[np.array(self.x_rightCam),np.array(self.y_rightCam)]

    def get_full_left_cam_features(self):
        return self.leftCamFeatures

    def get_full_right_cam_features(self):
        return self.rightCamFeatures

class AnimatedScatter(object):
    """An animated scatter plot using matplotlib.animations.FuncAnimation."""
    def __init__(self, num_points=16):
        self.num_points = num_points
        self.cam_object = IR_camera_object()
        # Setup the figure and axes...
        self.fig, self.ax = plt.subplots(1,2)
        self.point_size = 10
        self.color = 'blue'
        # Then setup FuncAnimation.
        self.ani = animation.FuncAnimation(self.fig, self.update, interval=20, 
                                          init_func=self.setup_plot, blit=True)

    def setup_plot(self):
        """Initial drawing of the scatter plot."""
        x, y, s, c = np.zeros(16),np.zeros(16),[self.point_size for s in range(self.num_points)],[self.color for c in range(self.num_points)] 
        self.scat_left = self.ax[0].scatter(x, y, c=c, s=s, vmin=0, vmax=1,
                                    cmap="jet", edgecolor="k")
        self.ax[0].axis([0, 128, 0, 128])
        self.scat_right = self.ax[1].scatter(x, y, c=c, s=s, vmin=0, vmax=1,
                                    cmap="jet", edgecolor="k")
        self.ax[1].axis([0, 128, 0, 128])
        return self.scat_left,self.scat_right

    def update(self,i):
        """Update the scatter plot."""
        self.cam_object = get_IR_data_from_stereo_camera(inp='f')
        if self.cam_object:
            left_data = self.cam_object.get_leftCam_obj_coordinates()
            right_data = self.cam_object.get_rightCam_obj_coordinates()
            # Set x and y data...
            self.scat_left.set_offsets(left_data[:, :2])
            self.scat_right.set_offsets(right_data[:, :2])
        # Set sizes...
        # self.scat.set_sizes(10*[10 for i in range(self.num_points)]
        # # Set colors..
        # self.scat.set_array(data[:, 3])

        # We need to return the updated artist for FuncAnimation to draw..
        # Note that it expects a sequence of artists, thus the trailing comma.
        return self.scat_left,self.scat_right

def get_features_object(data):
    # For First IR camera
    cam_object = IR_camera_object()
    #pprint(cam_object.get_full_left_cam_features())
    #First 256 bytes (16 bytes * 16 objects)
    for i in range(1,17):
        obj_dict = calculate_object_details(data[16*(i-1)+4:16*i+4])
        cam_object.set_left_cam_features(obj_dict)   

    #Second 256 bytes (16 bytes * 16 objects)
    for i in range(17,32):
        obj_dict = calculate_object_details(data[16*(i-1)+4:16*i+4])
        cam_object.set_right_cam_features(obj_dict)

    # set camera objects for the plot
    return cam_object


# Get a 16 byte data and convert into individual params 
def calculate_object_details(data):
    obj_dict = {}     
    area_low_byte = data[0]
    area_high_byte = int.from_bytes(data[1],byteorder='big') & int.from_bytes(b'\x3f',byteorder='big') #bytes(data[1] & b'\x3f'
    obj_dict['area'] =  int.from_bytes((area_high_byte).to_bytes(1, byteorder='big')+area_low_byte,\
                                        byteorder='big')
    center_x_low_byte = data[2]
    center_x_high_byte = int.from_bytes(data[3],byteorder='big') & int.from_bytes(b'\x3f',byteorder='big')
    obj_dict['center_x'] = int.from_bytes((center_x_high_byte).to_bytes(1, byteorder='big') + bytes(center_x_low_byte),\
                                            byteorder='big')
    center_y_low_byte = data[4]
    center_y_high_byte = int.from_bytes(data[5],byteorder='big') & int.from_bytes(b'\x3f',byteorder='big')
    obj_dict['center_y'] = int.from_bytes((center_y_high_byte).to_bytes(1, byteorder='big') + bytes(center_y_low_byte),\
                                            byteorder='big')
    obj_dict['avg_brightness'] = int.from_bytes(data[6],\
                                            byteorder='big')
    obj_dict['max_brightness'] = int.from_bytes(data[7],\
                                            byteorder='big')
    obj_dict['range'] = int.from_bytes(data[8],byteorder='big')>>4 & int.from_bytes(b'\x0f',byteorder='big')
    obj_dict['radius'] = int.from_bytes(data[8],byteorder='big') & int.from_bytes(b'\x0f',byteorder='big')
    obj_dict['boundary_left'] = int.from_bytes(data[9],byteorder='big') & int.from_bytes(b'\x7f',byteorder='big')
    obj_dict['boundary_right'] = int.from_bytes(data[10],byteorder='big') & int.from_bytes(b'\x7f',byteorder='big')
    obj_dict['boundary_up'] = int.from_bytes(data[11],byteorder='big') & int.from_bytes(b'\x7f',byteorder='big')
    obj_dict['boundary_down'] = int.from_bytes(data[12],byteorder='big') & int.from_bytes(b'\x7f',byteorder='big')
    obj_dict['aspect_ratio'] = int.from_bytes(data[13],\
                                            byteorder='big')
    obj_dict['bar_object_x'] = int.from_bytes(data[14],\
                                            byteorder='big')
    obj_dict['bar_object_y'] = int.from_bytes(data[15],\
                                            byteorder='big')
    return obj_dict

def get_IR_data_from_stereo_camera(inp='f'):
    if inp == 'exit':
        ser.close()
        exit()
    else:
        # send the character to the device
        # (note that I happend a \r\n carriage return and line feed to the characters - this is requested by my device)
        ser.write(str.encode(inp + '\r\n'))
        out = []
        # let's wait one second before reading output (let's give device time to answer)
        cnt =0
        while ser.inWaiting() > 0:
            #out += ser.read(1)
            out.append(ser.read(1))#.to_bytes(1,byteorder='big'))
            cnt = cnt+1
        print(cnt)
        if cnt ==516:
            cam_obj = get_features_object(out)
            return cam_obj
        else:
            return []

if __name__ == "__main__"
    # configure the serial connections (the parameters differs on the device you are connecting to)
    ser = serial.Serial(
        port='COM7',
        baudrate=115200,
        parity=serial.PARITY_ODD,
        stopbits=serial.STOPBITS_TWO,
        bytesize=serial.SEVENBITS
    )

    ser.isOpen()

    print ('Enter your commands below.\r\nInsert "exit" to leave the application.')

    a = AnimatedScatter()
    plt.show()

