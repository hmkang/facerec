//
//  ViewController.m
//  Face Recognition Library
//
//  Created by Pedro Centieiro on 3/28/12.
//  Copyright (c) 2012. All rights reserved.
//

#import "opencv2/opencv.hpp"
#import "opencv2/highgui/highgui.hpp"
#import "ViewController.h"
#import "facerec.hpp"

#define MAX_FACE_FILE 10

@interface ViewController ()

@end

@implementation ViewController
@synthesize vidUIView;
@synthesize authLabel;
@synthesize savedLabel;
@synthesize resetButton, resampleButton;

- (IplImage *)CreateIplImageFromUIImage:(UIImage *)image {
    // Getting CGImage from UIImage
    CGImageRef imageRef = image.CGImage;
    
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    // Creating temporal IplImage for drawing
    IplImage *iplimage = cvCreateImage(
                                       cvSize(image.size.width,image.size.height), IPL_DEPTH_8U, 4
                                       );
    // Creating CGContext for temporal IplImage
    CGContextRef contextRef = CGBitmapContextCreate(
                                                    iplimage->imageData, iplimage->width, iplimage->height,
                                                    iplimage->depth, iplimage->widthStep,
                                                    colorSpace, kCGImageAlphaPremultipliedLast|kCGBitmapByteOrderDefault
                                                    );
    // Drawing CGImage to CGContext
    CGContextDrawImage(
                       contextRef,
                       CGRectMake(0, 0, image.size.width, image.size.height),
                       imageRef
                       );
    CGContextRelease(contextRef);
    CGColorSpaceRelease(colorSpace);
    
    // Creating result IplImage
    IplImage *ret = cvCreateImage(cvGetSize(iplimage), IPL_DEPTH_8U, 3);
    cvCvtColor(iplimage, ret, CV_RGBA2BGR);
    cvReleaseImage(&iplimage);
    
    return ret;
}

-(IBAction) onReset:(id)sender {
    UIButton *btn = (UIButton *)sender;
    NSInteger tag = [sender tag];
    if(tag==22){ //resample
        savedIndex = 0;
        delete modelFisher;
        delete modelEigen;
        delete modelLBPH;
    }
    [savedLabel setText:btn.currentTitle];
    [resetButton setEnabled:false];
    [session startRunning];
}

- (void)initFaceRecognition {
    /*
    NSString* documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSString *filePath = [documentsPath stringByAppendingPathComponent:@"faces.model"];
    BOOL fileExists = [[NSFileManager defaultManager] fileExistsAtPath:filePath];
    if(!fileExists) {
        images.clear();
        labels.clear();
        for (int i=0; i<400; i+=10) {
            Mat src = [self CreateIplImageFromUIImage:[UIImage imageNamed:[NSString stringWithFormat:@"face_%04d.jpg", i]]];
            Mat dst;
            cv::cvtColor(src, dst, CV_BGR2GRAY);
                
            images.push_back(dst);
            labels.push_back(i);
            NSLog(@"image: %lu", images.size());
        }
        for (int i=0; i<10; i++) {
            Mat src = [self CreateIplImageFromUIImage:[UIImage imageNamed:[NSString stringWithFormat:@"%d.jpg", i]]];
            Mat dst;
            cv::cvtColor(src, dst, CV_BGR2GRAY);
            
            images.push_back(dst);
            labels.push_back(1000+i);
            NSLog(@"image: %lu", images.size());
        }

        model = new Fisherfaces(images, labels);
        model->save([filePath UTF8String]);
    } else {
        model = new Fisherfaces();
        model->load([filePath UTF8String]);        
    }
     */
    images.clear();
    labels.clear();
    NSString* documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    for (int i=0; i<MAX_FACE_FILE; i++) {
        NSString *fileName= [NSString stringWithFormat: @"%d.jpg", i];
        NSString *filePath = [documentsPath stringByAppendingPathComponent:fileName];
        UIImage *image = [UIImage imageWithContentsOfFile:filePath];
         /*
        CGRect rect = CGRectMake(i*30, i*30, 100, 100);
        UIImageView *imageView = [[UIImageView alloc] initWithFrame: rect];
        imageView.image = image;
        [vidUIView addSubview:imageView];
         */
        Mat src = [self CreateIplImageFromUIImage:image];
        Mat dst;
        cv::cvtColor(src, dst, CV_BGR2GRAY);
        
        images.push_back(dst);
        labels.push_back(i);
        NSLog(@"image: %lu", images.size());
        /*
        for(int j=1; j<=3; j++) {
            Mat src = [self CreateIplImageFromUIImage:[UIImage imageNamed:[NSString stringWithFormat:@"%d_%d.jpg", i, j]]];
            Mat dst;
            cv::cvtColor(src, dst, CV_BGR2GRAY);
            
            images.push_back(dst);
            labels.push_back(i);
            NSLog(@"image: %lu", images.size());
        }
        */
    }
    //[self stopCapture];
    modelEigen = new Eigenfaces(images, labels);
    modelFisher = new Fisherfaces(images, labels);
    modelLBPH = new LBPH(images, labels);
}

- (void)initFaceDetection
{
    NSString* documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSString *filePath = [documentsPath stringByAppendingPathComponent:[NSString stringWithFormat:@"%d.jpg", MAX_FACE_FILE-1]];
    BOOL fileExists = [[NSFileManager defaultManager] fileExistsAtPath:filePath];
    if(!fileExists) {
        savedIndex = 0;
    } else {
        savedIndex = 0; //MAX_FACE_FILE;        
    }
    NSLog(@"Total saved files: %d", savedIndex);
    //used in the captureoutput method below.
    detector = [CIDetector detectorOfType:CIDetectorTypeFace 
                                  context:nil options:[NSDictionary dictionaryWithObject:CIDetectorAccuracyLow forKey:CIDetectorAccuracy]];
}

- (void)viewDidLoad
{
    //[self initFaceRecognition];
    [self initFaceDetection];
    [super viewDidLoad];
    [resetButton setEnabled:false];
    [resetButton setTag:11];
    [resampleButton setTag:22];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    delete modelFisher;
    delete modelEigen;
    delete modelLBPH;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
}

- (void)startCapture
{
    [vidUIView.layer addSublayer:vidLayer];
    [session startRunning];
}

- (void)stopCapture
{
    [session stopRunning];    
    [vidLayer removeFromSuperlayer];
}


- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    //AVCapture instance wraps the AVCcaptureDevice and AVCaptureDeviceInput and AVCaptureOutput into a viewing session.
    session = [[AVCaptureSession alloc] init];
    //Chosing the Mediatype to Accept (Change The last parameter to get audio.
    NSArray *videoDevices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    AVCaptureDevice *captureDevice = nil;
    for (AVCaptureDevice *device in videoDevices)
    {
        //when it iterates to the dront camera set capturedevice to the front camera
        if (device.position == AVCaptureDevicePositionFront)
        {
            captureDevice = device;
            break;
        }
    }
    
    NSLog(@"%@", captureDevice);
    
    NSError *error = nil;
    
    //Takes the AVCaptureDevice as a parameter to create an input.
    AVCaptureDeviceInput *vidInput = [AVCaptureDeviceInput deviceInputWithDevice:captureDevice error:&error];
    
    
    AVCaptureVideoDataOutput * vidOutput = [[AVCaptureVideoDataOutput alloc] init];
    [vidOutput setAlwaysDiscardsLateVideoFrames:YES];
    [vidOutput setVideoSettings:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(id)kCVPixelBufferPixelFormatTypeKey]]; 	
    
    [vidOutput setSampleBufferDelegate:self queue:dispatch_get_main_queue()];
    
    
    //Preview layer is kind of an output, but not really. The output is generally a file of some sort (movie, recording, etc)
    vidLayer = [AVCaptureVideoPreviewLayer layerWithSession:session];
    
    //Making the Frame
    viewRect = CGRectMake(0, 20, 320, 410);
    vidLayer.frame = viewRect;
    
    [session addInput:vidInput];
    [session addOutput:vidOutput];

    // start capture
    [self startCapture];
}

#define degreesToRadians(degrees) ((degrees)/180.0 * M_PI)


- (UIImage *)rotateImage:(UIImage *)image degree:(CGFloat)degrees
{
    UIView *rotatedViewBox = [[UIView alloc] initWithFrame:CGRectMake(0,0,image.size.width, image.size.height)];
    CGAffineTransform t = CGAffineTransformMakeRotation(degreesToRadians(degrees));
    rotatedViewBox.transform = t;
    CGSize rotatedSize = rotatedViewBox.frame.size;
    
    UIGraphicsBeginImageContext(rotatedSize);
    CGContextRef bitmap = UIGraphicsGetCurrentContext();
    
    CGContextTranslateCTM(bitmap, rotatedSize.width/2, rotatedSize.height/2);
    
    CGContextRotateCTM(bitmap, degreesToRadians(degrees));
    
    CGContextScaleCTM(bitmap, 1.0, -1.0);
    CGContextDrawImage(bitmap, CGRectMake(-image.size.width / 2, -image.size.height / 2, image.size.width, image.size.height), [image CGImage]);
    
    UIImage *newImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    
    return newImage;
}

- (void)saveFaceWithFrame:(CGRect)rect image:(UIImage *)image
{
    UIGraphicsBeginImageContext(rect.size);
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGRect cripToRect = CGRectMake(0, 0, rect.size.width, rect.size.height);
    CGContextClipToRect(context, cripToRect);
    
    CGRect drawRect = CGRectMake(rect.origin.x *-1, rect.origin.y*-1, image.size.width, image.size.height);
    CGContextDrawImage(context, drawRect, image.CGImage);
    
    // crop and rotate
    UIImage *cropImage = UIGraphicsGetImageFromCurrentImageContext();
    UIImage *rotateImage = [self rotateImage:cropImage degree:90];

    // resize
    CGRect resizeRect = CGRectMake(0, 0, 100, 100);
    UIGraphicsBeginImageContext(resizeRect.size);
    [rotateImage drawInRect:resizeRect];
    UIImage *resizeImage = UIGraphicsGetImageFromCurrentImageContext();
    
    NSString* documentsPath = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSString *filePath = [documentsPath stringByAppendingPathComponent:[NSString stringWithFormat:@"%d.jpg", 
                                                                        savedIndex++]];
    NSData *data = [NSData dataWithData:UIImageJPEGRepresentation(resizeImage, 1.0)];
	[data writeToFile:filePath atomically:YES];

    UIGraphicsEndImageContext();
}

- (int)recognizeFaceWithFrame:(CGRect)rect image:(UIImage *)image
{
    UIGraphicsBeginImageContext(rect.size);
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGRect cripToRect = CGRectMake(0, 0, rect.size.width, rect.size.height);
    CGContextClipToRect(context, cripToRect);
    
    CGRect drawRect = CGRectMake(rect.origin.x *-1, rect.origin.y*-1, image.size.width, image.size.height);
    CGContextDrawImage(context, drawRect, image.CGImage);
    
    // crop and rotate
    UIImage *cropImage = UIGraphicsGetImageFromCurrentImageContext();
    UIImage *rotateImage = [self rotateImage:cropImage degree:90];
    
    // resize
    CGRect resizeRect = CGRectMake(0, 0, 100, 100);
    UIGraphicsBeginImageContext(resizeRect.size);
    [rotateImage drawInRect:resizeRect];
    UIImage *resizeImage = UIGraphicsGetImageFromCurrentImageContext();
    
    // get test instances
    cv::Mat srcSample = [self CreateIplImageFromUIImage:resizeImage];
    cv::Mat testSample;
    cv::cvtColor(srcSample, testSample, CV_BGR2GRAY);
    
    // test model
    int fisherValue = modelFisher->predict(testSample);
    int eigenValue = modelEigen->predict(testSample);
    int lbphValue = modelLBPH->predict(testSample);
    int ret = 1;
    if(fisherValue < 0 || eigenValue < 0 || lbphValue < 0) {
        ret = 0;
    }
    /*
    fisherValue = fisherValue < 0 ? 0 : fisherValue;
    eigenValue = eigenValue < 0 ? 0 : eigenValue;
    lbphValue = lbphValue < 0 ? 0 : lbphValue;
    NSString *fisherName = [nameArray objectAtIndex: fisherValue];
    NSString *eigenName = [nameArray objectAtIndex: eigenValue];
    NSString *lbphName = [nameArray objectAtIndex: lbphValue];
    NSLog(@"%@, %@, %@", fisherName, eigenName, lbphName);
    
    [authLabel setText:[NSString stringWithFormat:@"Fisher:%@, Eigen:%@, LBPH:%@", 
                        fisherName, eigenName, lbphName]];
    */
    UIGraphicsEndImageContext();
    return ret;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection{
    
    
    //create a pixelbuffer from the sample buffer
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(sampleBuffer);
    //Create a ciimage from the pixelbuffer
    ciimage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
    
    //scales the image that is seen by the cidetector down
    CIImage *myCiImage = [ciimage imageByApplyingTransform:CGAffineTransformMakeScale(.73, -.68)];
    //CIImage *myCiImage = [ciimage imageByApplyingTransform:CGAffineTransformMakeScale(.43, -.40)];
    //because the scaling for y is negative it mirrors on the left side. I then have to translate (move) the image
    //over by its width.
    CIImage *transImage = [myCiImage imageByApplyingTransform:CGAffineTransformMakeTranslation(0, 320)];
    
    //options for features array below. It changes the orientation of the image 90 degrees(option 6)
    NSDictionary *imageOptions = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:6]
                                                             forKey:CIDetectorImageOrientation];
    
    // set label
    //[authLabel setText:[NSString stringWithFormat:@"Ready"]];
    
    //grabs features from detectore wich is implemented in viewdidload above. uses he options above
    NSArray *features = [detector featuresInImage:transImage options:imageOptions];
    
    for(CIFaceFeature *ff in features){
        //remove face rectangle from view before adding another one.
        //[faceView removeFromSuperview];
        //[mouth removeFromSuperview];
        //flip X and Y for the face box
        CGFloat faceY = ff.bounds.origin.y;
        CGFloat faceX = ff.bounds.origin.x;
        CGFloat faceHeight = ff.bounds.size.height;
        CGFloat faceWidth = ff.bounds.size.width;
        //make rectangle for face based on face bounds and position from cidetector
        CGRect featuresForSave = CGRectMake(faceX ,faceY , faceWidth, faceHeight);
        CGRect featuresForView = CGRectMake(faceY ,faceX , faceWidth, faceHeight);
        //////
        CIContext *context = [CIContext contextWithOptions:nil];    
        UIImage *image = [UIImage imageWithCGImage:[context createCGImage:transImage fromRect:transImage.extent]];
        if(savedIndex < MAX_FACE_FILE){            
            [self saveFaceWithFrame:featuresForSave image:image];
            [authLabel setText:[NSString stringWithFormat:@"Saved %d", savedIndex]];
            NSLog(@"Saved %d", savedIndex);
        } else if (savedIndex == MAX_FACE_FILE) {
            [self initFaceRecognition];
            [savedLabel setText:@"Sampling Done."];
            [resetButton setEnabled:true];
            savedIndex++;
        } else {
            //NSLog(@"Face recognition: %f%c%f", faceWidth, 'x', faceHeight);
            int recognizedOrNot = [self recognizeFaceWithFrame:featuresForSave image:image];
            if(recognizedOrNot==1){
                [authLabel setText:@"Matched"];
                [session stopRunning];
                [resetButton setEnabled:true];
            } else {
                [authLabel setText:@"Not matched"];  
            }
            //[authLabel setText:[NSString stringWithFormat:@"predict: %d", fIndex]];
            /*
            if(fIndex>=100) {
                [self stopCapture];

                CGRect labelRect = CGRectMake(10, 10, 200, 30);
                UILabel *authLabel = [[UILabel alloc] initWithFrame: labelRect];
                [authLabel setText:@"Authenticated."];
                [vidUIView addSubview:authLabel];
                break;
            }
            */
        }
        //////
        
        //NSLog(@"feature x %f, y %f", faceX, faceY);
        
        //Using if with BOOL to create the red face box once then move it around afterwards.
        if(createdFaceBox == NO){
            //Create face Box.
            faceView = [[UIView alloc] initWithFrame:featuresForView];
            // add a border around the newly created UIView
            faceView.layer.borderWidth = 1;
            faceView.layer.borderColor = [[UIColor redColor] CGColor];
            //add facerect to uiview
            [vidUIView addSubview:faceView];
            createdFaceBox = YES;
        }else{
            [faceView setFrame:featuresForView];
        }
        /*
         if(ff.hasMouthPosition)
         {
         // create a UIView with a size based on the width of the face
         mouth = [[UIView alloc] initWithFrame:CGRectMake(ff.mouthPosition.y, ff.mouthPosition.x, faceWidth*0.4, faceWidth*0.1)];
         // change the background color for the mouth to green
         mouth.backgroundColor = [UIColor greenColor];
         // create a cgpoint with the x set to the y axis and visa versa.
         CGPoint mouthPos = CGPointMake(ff.mouthPosition.y, ff.mouthPosition.x);
         // set the position of the mouthView based on the face
         [mouth setCenter:mouthPos];
         // round the corners
         mouth.layer.cornerRadius = faceWidth*0.2;
         // add the new view to the window
         [vidUIView addSubview:mouth];
         }    
         */
    }
}

@end
