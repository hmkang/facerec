//
//  ViewController.h
//  Face Recognition Library
//
//  Created by Pedro Centieiro on 3/28/12.
//  Copyright (c) 2012. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import "opencv2/opencv.hpp"
#import "facerec.hpp"

@interface ViewController : UIViewController <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    UIView *vidUIView;
    UILabel *authLabel;
    UIButton *resetButton;
    UIButton *resampleButton;
    UILabel *savedLabel;
    
    UIView *faceView;   //for face rectangle
    NSInteger savedIndex;
    CGRect viewRect;
    BOOL createdFaceBox;
    dispatch_queue_t queue;
    AVCaptureSession *session;
    CIImage *ciimage;
    CIDetector *detector;
    AVCaptureVideoPreviewLayer *vidLayer;
    vector<Mat> images;
    vector<int> labels;
    Fisherfaces *modelFisher;
    Eigenfaces *modelEigen;
    LBPH *modelLBPH;
    NSArray *nameArray;
}

@property (strong, nonatomic) IBOutlet UIView *vidUIView;
@property (strong, nonatomic) IBOutlet UIView *authLabel;
@property (strong, nonatomic) IBOutlet UIView *savedLabel;
@property (strong, nonatomic) IBOutlet UIButton *resetButton;
@property (strong, nonatomic) IBOutlet UIButton *resampleButton;

@end
